#include "../inc/dimmer.h"
#include "../inc/upower.h"
#include "../inc/brightness.h"
#include "../inc/bus.h"
#include <sys/inotify.h>

#define BUF_LEN (sizeof(struct inotify_event) + NAME_MAX + 1)

static void init(void);
static int check(void);
static void destroy(void);
static void dimmer_cb(void);
static void start_dim_timer(void);
static void disarm_dim_timer(void);
static void dim_backlight(__attribute__((unused)) union sigval sv);
static int get_idle_time(void);
static void upower_callback(int old_state);

static int inot_wd, inot_fd, timer_fd, dimmed_br;
static timer_t timerid;
static struct dependency dependencies[] = { {SOFT, UPOWER}, {HARD, BRIGHTNESS}, {HARD, BUS} };
static struct self_t self = {
    .name = "Dimmer",
    .idx = DIMMER,
    .num_deps = SIZE(dependencies),
    .deps =  dependencies
};

void set_dimmer_self(void) {
    modules[self.idx].self = &self;
    modules[self.idx].init = init;
    modules[self.idx].check = check;
    modules[self.idx].destroy = destroy;
    set_self_deps(&self);
}

static void init(void) {
    inot_fd = inotify_init();
    if (inot_fd != -1) {
        timer_fd = start_timer(CLOCK_MONOTONIC, conf.dimmer_timeout[state.ac_state], 0);
        init_module(timer_fd, self.idx, dimmer_cb);
        if (!modules[self.idx].disabled) {
            /* create timer for smooth transitions if needed */
            if (!conf.no_dimmer_smooth_transition) {
                struct sigevent sevp = {0};
            
                sevp.sigev_notify = SIGEV_THREAD;
                sevp.sigev_notify_function = dim_backlight;
                if (timer_create(CLOCK_MONOTONIC, &sevp, &timerid)) {
                    ERROR("%s\n", strerror(errno));
                }
            }
            
            add_upower_module_callback(upower_callback);
            /* brightness module is started before dimmer, so state.br.max is already ok there */
            dimmed_br = (double)state.br.max * conf.dimmer_pct / 100;
        }
    }
}

/* Check we're on X */
static int check(void) {
    return  conf.single_capture_mode ||
    conf.no_dimmer || !state.display || !state.xauthority;
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
    if (timerid) {
        timer_delete(timerid);
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
static void dimmer_cb(void) {
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
                    /* Don't touch backlight if a lower level is already set */
                    if (dimmed_br >= state.br.old) {
                        DEBUG("A lower than dimmer_pct backlight level is already set. Avoid changing it.\n");
                    } else {
                        if (conf.no_dimmer_smooth_transition) {
                            set_backlight_level(dimmed_br);
                        } else {
                            start_dim_timer();
                        }
                    }
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
            if (!conf.no_dimmer_smooth_transition && timerid) {
                disarm_dim_timer();
            }
            main_p[self.idx].fd = timer_fd;
            set_timeout(conf.dimmer_timeout[state.ac_state], 0, main_p[self.idx].fd, 0);
            /* restore previous backlight level */
            set_backlight_level(state.br.old);
        }
    }
}

static void start_dim_timer(void) {
    struct itimerspec timerValue = {{0}};
    
    timerValue.it_value.tv_nsec = 1; // start immediately
    timerValue.it_interval.tv_nsec = 30 * 1000 * 1000; // 30 ms interval
    if (timer_settime(timerid, 0, &timerValue, NULL)) {
        ERROR("%s\n", strerror(errno));
    }
    state.br.current = state.br.old;
}

static void disarm_dim_timer(void) {
    struct itimerspec timerValue = {{0}};
    
    if (timer_settime(timerid, 0, &timerValue, NULL)) {
        ERROR("%s\n", strerror(errno));
    }
}

static void dim_backlight(__attribute__((unused)) union sigval sv) {
    const int lower_br = (double)state.br.current - (double)state.br.max / 20; // 5% steps
    set_backlight_level(lower_br > dimmed_br ? lower_br : dimmed_br);
    if (state.br.current == dimmed_br) {
        disarm_dim_timer();
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
static void upower_callback(int old_state) {
    if (!state.is_dimmed) {
        reset_timer(main_p[self.idx].fd, conf.dimmer_timeout[old_state], conf.dimmer_timeout[state.ac_state]);
    }
}
