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
static void dim_backlight(void);
static int get_idle_time(void);
static void upower_callback(int old_state);

static int inot_wd, inot_fd, timer_fd, bright_fd;
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
            add_upower_module_callback(upower_callback);
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
        
        int idle_t = get_idle_time();
        if (idle_t > 0) {
            /* -1 as it seems we receive events circa 1s before */
            state.is_dimmed = idle_t >= conf.dimmer_timeout[state.ac_state];
            if (state.is_dimmed) {
                inot_wd = inotify_add_watch(inot_fd, "/dev/input/", IN_ACCESS | IN_ONESHOT);
                if (inot_wd != -1) {
                    main_p[self.idx].fd = inot_fd;
                    bright_fd = main_p[BRIGHTNESS].fd;
                    /* stop listening BRIGHTNESS module fd */
                    main_p[BRIGHTNESS].fd = 0;
                    dim_backlight();
                } else {
                    // in case of error, reset is_dimmed state
                    state.is_dimmed = 0;
                }
            } else {
                /* Set a timeout of conf.dimmer_timeout - elapsed time since latest user activity */
                set_timeout(conf.dimmer_timeout[state.ac_state] - idle_t, 0, main_p[self.idx].fd, 0);
            }
        }
    } else {
        char buffer[BUF_LEN];
        int length = read(main_p[self.idx].fd, buffer, BUF_LEN);
        if (length > 0) {
            state.is_dimmed = 0;
            main_p[self.idx].fd = timer_fd;
            set_timeout(conf.dimmer_timeout[state.ac_state], 0, main_p[self.idx].fd, 0);
            /* restore previous backlight level */
            set_backlight_level(state.br.old);
            /* restart listening BRIGHTNESS module fd */
            main_p[BRIGHTNESS].fd = bright_fd;
        }
    }
}

static void dim_backlight(void) {
    int lowered_br = state.br.max * conf.dimmer_pct / 100;
    // store old brightness
    get_current_brightness();
    
    if (lowered_br < state.br.old) {
        set_backlight_level(lowered_br);
    } else {
        DEBUG("A lower than dimmer_pct backlight level is already set. Avoid changing it.\n");
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
