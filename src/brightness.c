#include "../inc/brightness.h"
#include "../inc/dpms.h"

static void init(void);
static void destroy(void);
static void brightness_cb(void);
static void do_capture(void);
static void get_max_brightness(void);
static void get_current_brightness(void);
static void set_brightness(double perc);
static double capture_frames_brightness(void);

/*
 * Storage struct for our needed variables.
 */
struct brightness {
    int current;
    int max;
    int old;
};

static struct brightness br;
static struct dependency dependencies[] = { {HARD, BUS_IX}, {SOFT, GAMMA_IX} };
static struct self_t self = {
    .name = "Brightness",
    .idx = CAPTURE_IX,
    .num_deps = SIZE(dependencies),
    .deps =  dependencies
};

void set_brightness_self(void) {
    modules[self.idx].self = &self;
    modules[self.idx].init = init;
    modules[self.idx].destroy = destroy;
    set_self_deps(&self);
}

/*
 * Init brightness values (max and current)
 */
static void init(void) {
    get_max_brightness();
    if (!state.quit) {
        int fd = start_timer(CLOCK_MONOTONIC, 1, 0);
        init_module(fd, self.idx, brightness_cb);
    }
}

static void destroy(void) {
    if (main_p[self.idx].fd > 0) {
        close(main_p[self.idx].fd);
    }
}

static void brightness_cb(void) {
    if (!conf.single_capture_mode) {
        uint64_t t;
        read(main_p[self.idx].fd, &t, sizeof(uint64_t));
    }
    do_capture();
    if (conf.single_capture_mode) {
        state.quit = 1;
    }
}

/**
 * When timerfd timeout expires, check if we are in screen power_save mode,
 * otherwise start streaming on webcam and set CAMERA_IX fd of pollfd struct to
 * webcam device fd. This way our main poll will get events (frames) from webcam device too.
 */
static void do_capture(void) {
    static const int fast_timeout = 15;
    static const double drop_limit = 0.6;

    /*
     * if screen is currently blanked thanks to dpms,
     * do not do anything. Set a long timeout and return.
     * Timeout will increase as screen power management goes deeper.
     */
    if (get_screen_dpms() > 0) {
        INFO("Screen is currently in power saving mode. Avoid changing brightness and setting a long timeout.\n");
        return set_timeout(2 * conf.timeout[state.time] * get_screen_dpms(), 0, main_p[self.idx].fd, 0);
    }

    double val = capture_frames_brightness();
    if (!state.quit && val >= 0.0) {
        INFO("Average frames brightness: %lf.\n", val);
        set_brightness(val);
        
        if (!conf.single_capture_mode && !state.quit) {
            double drop = (double)(br.current - br.old) / br.max;
            // if there is too high difference, do a fast recapture to be sure
            // this is the correct level
            if (fabs(drop) > drop_limit) {
                INFO("Weird brightness drop. Recapturing in 15 seconds.\n");
                // single call after 15s
                set_timeout(fast_timeout, 0, main_p[self.idx].fd, 0);
            } else {
                // reset normal timer
                set_timeout(conf.timeout[state.time], 0, main_p[self.idx].fd, 0);
            }
        }
    }
}

static void get_max_brightness(void) {
    struct bus_args args = {"org.clightd.backlight", "/org/clightd/backlight", "org.clightd.backlight", "getmaxbrightness"};
    bus_call(&br.max, "i", &args, "s", conf.screen_path);
}

static void get_current_brightness(void) {
    struct bus_args args = {"org.clightd.backlight", "/org/clightd/backlight", "org.clightd.backlight", "getbrightness"};
    bus_call(&br.old, "i", &args, "s", conf.screen_path);
}

static void set_brightness(double perc) {
    int new_br =  br.max * perc;
    // store old brightness
    get_current_brightness();
    if (state.quit) {
        return;
    }
    
    if (new_br != br.old) {
        INFO("Old brightness value: %d\n", br.old);
        struct bus_args args = {"org.clightd.backlight", "/org/clightd/backlight", "org.clightd.backlight", "setbrightness"};
        bus_call(&br.current, "i", &args, "si", conf.screen_path, new_br);
        if (!state.quit) {
            INFO("New brightness value: %d\n", br.current);
        }
    } else {
        INFO("Brightness level was already %d.\n", new_br);
    }
}

static double capture_frames_brightness(void) {
    double brightness = -1;
    struct bus_args args = {"org.clightd.backlight", "/org/clightd/backlight", "org.clightd.backlight", "captureframes"};
    bus_call(&brightness, "d", &args, "si", conf.dev_name, conf.num_captures);
    return brightness;
}
