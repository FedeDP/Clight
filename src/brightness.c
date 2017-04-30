#include "../inc/brightness.h"
#include "../inc/dpms.h"

static void init(void);
static void brightness_cb(void);
static void do_capture(void);
static void get_max_brightness(void);
static void get_current_brightness(void);
static void set_brightness(const double perc);
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
static struct dependency dependencies[] = { {HARD, BUS_IX}, {SOFT, GAMMA_IX}, {SOFT, UPOWER_IX} };
static struct self_t self = {
    .name = "Brightness",
    .idx = CAPTURE_IX,
    .num_deps = SIZE(dependencies),
    .deps =  dependencies
};

void set_brightness_self(void) {
    modules[self.idx].self = &self;
    modules[self.idx].init = init;
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

#ifdef DPMS_PRESENT
    /*
     * if screen is currently blanked thanks to dpms,
     * do not do anything. Set a long timeout and return.
     * Timeout will increase as screen power management goes deeper.
     */
    if (get_screen_dpms() > 0) {
        INFO("Screen is currently in power saving mode. Avoid changing brightness and setting a long timeout.\n");
        return set_timeout(2 * conf.timeout[state.time] * get_screen_dpms(), 0, main_p[self.idx].fd, 0);
    }
#endif

    double val = capture_frames_brightness();
    /* 
     * if captureframes clightd method did not return any non-critical error (eg: eperm).
     * I won't check setbrightness too because if captureframes did not return any error,
     * it is very very unlikely that setbrightness would return some.
     */
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

/*
 * Equation for 'b' found with a fitting around these points:
 * X = 0.0  Y = 0.0          
 *     0.1      0.15      
 *     0.2      0.29       
 *     0.3      0.45       
 *     0.4      0.61       
 *     0.5      0.74       
 *     0.6      0.81       
 *     0.7      0.88       
 *     0.8      0.93       
 *     0.9      0.97       
 *     1.0      1
 * Where X is ambient brightness and Y is backlight level.
 * Empirically built (fast growing curve for lower values, and flattening m for values near 1)
 */
static void set_brightness(const double perc) {
    const double b = 1.319051 + (0.008722895 - 1.319051) / (1 + pow((perc/0.4479636), 1.540376));
    int new_br =  br.max * b;
    // store old brightness
    get_current_brightness();
    if (state.quit) {
        return;
    }
    
    if (new_br != br.old) {
        INFO("Old brightness value: %d\n", br.old);
        struct bus_args args = {"org.clightd.backlight", "/org/clightd/backlight", "org.clightd.backlight", "setbrightness"};
        bus_call(&br.current, "i", &args, "si", conf.screen_path, new_br >= conf.lowest_backlight_level ? new_br : conf.lowest_backlight_level);
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
