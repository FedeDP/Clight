#include "../inc/dimmer.h"
#include "../inc/brightness.h"
#include "../inc/dimmer_smooth.h"
#include "../inc/bus.h"
#include <sys/inotify.h>

#define BUF_LEN (sizeof(struct inotify_event) + NAME_MAX + 1)

static void init(void);
static int check(void);
static void destroy(void);
static void callback(void);
static void dim_backlight(void);
static void restore_backlight(void);
static int get_idle_time(void);
static void upower_callback(const void *ptr);
static void inhibit_callback(const void * ptr);

static int inot_wd, inot_fd, timer_fd;
static struct dependency dependencies[] = { {SOFT, UPOWER}, {HARD, BRIGHTNESS}, {HARD, BUS}, {HARD, XORG}, {SOFT, INHIBIT} };
static struct self_t self = {
    .name = "Dimmer",
    .idx = DIMMER,
    .num_deps = SIZE(dependencies),
    .deps =  dependencies,
    .standalone = 1
};

void set_dimmer_self(void) {
    SET_SELF();
}

static void init(void) {
    inot_fd = inotify_init();
    if (inot_fd != -1) {
        struct bus_cb upower_cb = { UPOWER, upower_callback };
        struct bus_cb inhibit_cb = { INHIBIT, inhibit_callback };
        
        timer_fd = start_timer(CLOCK_MONOTONIC, state.pm_inhibited || conf.dimmer_timeout[state.ac_state] <= 0 ? 
                                0 : conf.dimmer_timeout[state.ac_state], 0); // Normal timeout if !inhibited AND dimmer timeout > 0, else disarmed
        INIT_MOD(timer_fd, &upower_cb, &inhibit_cb);
    }
}

/* Check we're on X */
static int check(void) {
    return 0;
}

static void destroy(void) {
    if (state.is_dimmed) {
        inotify_rm_watch(inot_fd, inot_wd);
        if (timer_fd > 0) {
            close(timer_fd);
        }
    } else if (inot_fd > 0) {
        close(inot_fd);
    }
}

/*
 * If is_dimmed is false, check how many seconds are passed since latest user activity.
 * If > of conf.dimmer_timeout, we're in dimmed state:
 *      add an inotify_watcher on /dev/input to be woken as soon as a new activity happens.
 *      Note the IN_ONESHOT flag, that is needed to just wait to capture a single inotify event.
 *      Then, stop listening on BRIGHTNESS module (as it is disabled while in dimmed state),
 *      and dim backlight.
 * Else set a timeout of conf.dimmer_timeout - elapsed time since latest user activity.
 *
 * Else, read single event from inotify, leave dimmed state,
 * resume BACKLIGHT module and reset latest backlight level. 
 */
static void callback(void) {
    if (!state.is_dimmed) {
        uint64_t t;
        read(main_p[self.idx].fd, &t, sizeof(uint64_t));
        
        /* If interface is not enabled, avoid entering dimmed state */
        int idle_t = get_idle_time();
        if (idle_t > 0) {
            state.is_dimmed = idle_t >= conf.dimmer_timeout[state.ac_state] - 1;
            if (state.is_dimmed) {
                inot_wd = inotify_add_watch(inot_fd, "/dev/input/", IN_ACCESS | IN_ONESHOT);
                if (inot_wd != -1) {
                    main_p[self.idx].fd = inot_fd;
                    // update state.br.old
                    get_current_brightness();
                    dim_backlight();
                } else {
                    // in case of error, reset is_dimmed state
                    state.is_dimmed = 0;
                }
            } else {
                /* Set a timeout of conf.dimmer_timeout - elapsed time since latest user activity */
                set_timeout(conf.dimmer_timeout[state.ac_state] - idle_t, 0, main_p[self.idx].fd, 0);
            }
        } else {
            set_timeout(conf.dimmer_timeout[state.ac_state], 0, main_p[self.idx].fd, 0);
        }
    } else {
        char buffer[BUF_LEN];
        int length = read(main_p[self.idx].fd, buffer, BUF_LEN);
        if (length > 0) {
            state.is_dimmed = 0;
            main_p[self.idx].fd = timer_fd;
            restore_backlight();
            set_timeout(conf.dimmer_timeout[state.ac_state] * !state.pm_inhibited, 0, main_p[self.idx].fd, 0);
        }
    }
}

static void dim_backlight(void) {
    /* Don't touch backlight if a lower level is already set */
    if (conf.dimmer_pct >= state.br_pct.old) {
        DEBUG("A lower than dimmer_pct backlight level is already set. Avoid changing it.\n");
    } else {
        if (is_inited(DIMMER_SMOOTH)) {
            state.br_pct.current = state.br_pct.old;
            start_smooth_transition(1);
        } else {
            set_backlight_level(conf.dimmer_pct);
        }
    }
}

/* restore previous backlight level */
static void restore_backlight(void) {
    if (is_inited(DIMMER_SMOOTH)) {
        start_smooth_transition(1);
    } else {
        set_backlight_level(state.br_pct.old);
    }
}

static int get_idle_time(void) {
    int idle_time = -1;
    struct bus_args args = {"org.clightd.backlight", "/org/clightd/backlight", "org.clightd.backlight", "getidletime"};
    call(&idle_time, "i", &args, "ss", state.display, state.xauthority);
    /* clightd returns ms of inactivity. We need seconds */
    return round(idle_time / 1000);
}

/* Reset dimmer timeout */
static void upower_callback(const void *ptr) {
    int old_ac_state = *(int *)ptr;
    /* Force check that we received an ac_state changed event for real */
    if (!state.is_dimmed && !state.pm_inhibited && old_ac_state != state.ac_state) {
        if (conf.dimmer_timeout[state.ac_state] <= 0) {
            set_timeout(0, 0, main_p[self.idx].fd, 0); // if timeout is <= 0, pause this module
        } else {
            reset_timer(main_p[self.idx].fd, conf.dimmer_timeout[old_ac_state], conf.dimmer_timeout[state.ac_state]);
        }
    }
}

/* 
 * If we're getting inhibited, disarm timer (pausing this module)
 * else restart module with its default timeout
 */
static void inhibit_callback(const void *ptr) {
    int old_pm_state = *(int *)ptr;
    if (!state.is_dimmed && old_pm_state != state.pm_inhibited && conf.dimmer_timeout[state.ac_state] > 0) {
        DEBUG("Dimmer module being %s.\n", state.pm_inhibited ? "paused" : "restarted");
        set_timeout(conf.dimmer_timeout[state.ac_state] * !state.pm_inhibited, 0, main_p[self.idx].fd, 0);
    }
}
