#include <brightness.h>
#include <bus.h>
#include <sys/inotify.h>
#include <interface.h>

#define BUF_LEN (sizeof(struct inotify_event) + NAME_MAX + 1)

static void dim_backlight(const double pct);
static void restore_backlight(const double pct);
static int get_idle_time(void);
static void upower_callback(const void *ptr);
static void inhibit_callback(const void * ptr);
static void interface_timeout_callback(const void *ptr);

static int inot_wd, inot_fd, timer_fd;

/*
 * DIMMER needs BRIGHTNESS as it needs to be sure that state.current_br_pct is correctly setted.
 * BRIGHTNESS will set state.current_br_pct at first capture (1ns after clight's startup)
 */
static struct dependency dependencies[] = { {SOFT, UPOWER}, {HARD, BRIGHTNESS}, {HARD, XORG}, {SOFT, INHIBIT}, {HARD, CLIGHTD}, {SOFT, INTERFACE} };
static struct self_t self = {
    .num_deps = SIZE(dependencies),
    .deps =  dependencies,
    .functional_module = 1
};

MODULE(DIMMER);

static void init(void) {
    struct bus_cb upower_cb = { UPOWER, upower_callback };
    struct bus_cb inhibit_cb = { INHIBIT, inhibit_callback };
    struct bus_cb interface_inhibit_cb = { INTERFACE, inhibit_callback, "inhibit" };
    struct bus_cb interface_to_cb = { INTERFACE, interface_timeout_callback, "dimmer_timeout" };
    
    timer_fd = DONT_POLL_W_ERR;
    inot_fd = inotify_init();
    if (inot_fd != -1) {
        timer_fd = start_timer(CLOCK_MONOTONIC, state.pm_inhibited || conf.dimmer_timeout[state.ac_state] <= 0 ? 
                                0 : conf.dimmer_timeout[state.ac_state], 0); // Normal timeout if !inhibited AND dimmer timeout > 0, else disarmed
    }
    INIT_MOD(timer_fd, &upower_cb, &inhibit_cb, &interface_inhibit_cb, &interface_to_cb);
}

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
    static double old_pct;
    
    if (!state.is_dimmed) {
        uint64_t t;
        read(main_p[self.idx].fd, &t, sizeof(uint64_t));
        
        /* If interface is not enabled, avoid entering dimmed state */
        int idle_t = get_idle_time();
        if (idle_t > 0) {
            state.is_dimmed = idle_t >= conf.dimmer_timeout[state.ac_state];
            if (state.is_dimmed) {
                DEBUG("Entering dimmed state...\n");
                inot_wd = inotify_add_watch(inot_fd, "/dev/input/", IN_ACCESS | IN_ONESHOT);
                if (inot_wd != -1) {
                    main_p[self.idx].fd = inot_fd;
                    old_pct = state.current_br_pct;
                    dim_backlight(conf.dimmer_pct);
                    emit_prop("Dimmed");
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
            DEBUG("Leaving dimmed state...\n");
            state.is_dimmed = 0;
            main_p[self.idx].fd = timer_fd;
            restore_backlight(old_pct);
            emit_prop("Dimmed");
            set_timeout(conf.dimmer_timeout[state.ac_state] * !state.pm_inhibited, 0, main_p[self.idx].fd, 0);
        }
    }
}

static void dim_backlight(const double pct) {
    /* Don't touch backlight if a lower level is already set */
    if (pct >= state.current_br_pct) {
        DEBUG("A lower than dimmer_pct backlight level is already set. Avoid changing it.\n");
    } else {
        set_backlight_level(pct, !conf.no_smooth_dimmer, conf.dimmer_trans_step, conf.dimmer_trans_timeout);
    }
}

/* restore previous backlight level */
static void restore_backlight(const double pct) {
    set_backlight_level(pct, !conf.no_smooth_backlight, conf.dimmer_trans_step, conf.dimmer_trans_timeout);
}

static int get_idle_time(void) {
    int idle_time;
    struct bus_args args = {"org.clightd.backlight", "/org/clightd/backlight", "org.clightd.backlight", "GetIdleTime"};
    int r = call(&idle_time, "i", &args, "ss", state.display, state.xauthority);
    if (!r) {
        /* clightd returns ms of inactivity. We need seconds */
        return lround((double)idle_time / 1000);
    }
    return r;
}

/* Reset dimmer timeout */
static void upower_callback(const void *ptr) {
    int old_ac_state = *(int *)ptr;
    /* Force check that we received an ac_state changed event for real */
    if (!state.is_dimmed && old_ac_state != state.ac_state) {
        reset_timer(main_p[self.idx].fd * !state.pm_inhibited, conf.dimmer_timeout[old_ac_state], conf.dimmer_timeout[state.ac_state]);
    }
}

/* 
 * If we're getting inhibited, disarm timer (pausing this module)
 * else restart module with its default timeout
 */
static void inhibit_callback(const void *ptr) {
    int old_pm_state = *(int *)ptr;
    if (!state.is_dimmed && !!old_pm_state != !!state.pm_inhibited) {
        DEBUG("Dimmer module being %s.\n", state.pm_inhibited ? "paused" : "restarted");
        set_timeout(conf.dimmer_timeout[state.ac_state] * !state.pm_inhibited, 0, main_p[self.idx].fd, 0);
    }
}

static void interface_timeout_callback(const void *ptr) {
    if (!state.is_dimmed) {
        int old_val = *((int *)ptr);
        reset_timer(main_p[self.idx].fd, old_val, conf.dimmer_timeout[state.ac_state]);
    }
}
