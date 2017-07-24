#include "../inc/dimmer.h"
#include "../inc/upower.h"
#include "../inc/brightness.h"
#include "../inc/dimmer_smooth.h"
#include <sys/inotify.h>

#define BUF_LEN (sizeof(struct inotify_event) + NAME_MAX + 1)

static void init(void);
static int check(void);
static void destroy(void);
static void callback(void);
static void dim_backlight(void);
static void restore_backlight(void);
static int get_idle_time(void);
static void upower_callback(void);

static int inot_wd, inot_fd, timer_fd;
static struct dependency dependencies[] = { {SOFT, UPOWER}, {HARD, BRIGHTNESS}, {HARD, BUS}, {HARD, XORG} };
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
        
        timer_fd = start_timer(CLOCK_MONOTONIC, 0, 1);
        INIT_MOD(timer_fd, &upower_cb);
        /* brightness module is started before dimmer, so state.br.max is already ok there */
        state.dimmed_br = (double)state.br.max * conf.dimmer_pct / 100;
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
        
        int idle_t = 0;
        
        /* If interface is not enabled, avoid entering dimmed state */
        if (is_interface_enabled()) {
            idle_t = get_idle_time();
        } else {
            INFO("Current backlight interface is not enabled. Avoid checking if screen must be dimmed.\n");
        }
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
            set_timeout(conf.dimmer_timeout[state.ac_state], 0, main_p[self.idx].fd, 0);
        }
    }
}

static void dim_backlight(void) {
    /* Don't touch backlight if a lower level is already set */
    if (state.dimmed_br >= state.br.old) {
        DEBUG("A lower than dimmer_pct backlight level is already set. Avoid changing it.\n");
    } else {
        if (is_disabled(DIMMER_SMOOTH)) {
            set_backlight_level(state.dimmed_br);
        } else if (is_inited(DIMMER_SMOOTH)) {
            state.br.current = state.br.old;
            start_smooth_transition(1);
        }
    }
}

/* restore previous backlight level */
static void restore_backlight(void) {
    if (is_disabled(DIMMER_SMOOTH)) {
        set_backlight_level(state.br.old);
    } else if (is_inited(DIMMER_SMOOTH)) {
        start_smooth_transition(1);
    }
}

static int get_idle_time(void) {
    int idle_time = -1;
    struct bus_args args = {"org.clightd.backlight", "/org/clightd/backlight", "org.clightd.backlight", "getidletime"};
    bus_call(&idle_time, "i", &args, "ss", state.display, state.xauthority);
    /* clightd returns ms of inactivity. We need seconds */
    return round(idle_time / 1000);
}

/* Reset dimmer timeout */
static void upower_callback(void) {
    /* Force check that we received an ac_state changed event for real */
    if (!state.is_dimmed && state.old_ac_state != state.ac_state) {
        reset_timer(main_p[self.idx].fd, conf.dimmer_timeout[state.old_ac_state], conf.dimmer_timeout[state.ac_state]);
    }
}
