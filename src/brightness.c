#include "../inc/brightness.h"
#include "../inc/dpms.h"

static void brightness_cb(void);
static void do_capture(void);
static void get_max_brightness(void);
static void get_current_brightness(void);
static double set_brightness(double perc);
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

/*
 * Init brightness values (max and current)
 */
void init_brightness(void) {
    get_max_brightness();
    if (!state.quit) {
        get_current_brightness();
        if (!state.quit) {
            int fd = start_timer(CLOCK_MONOTONIC, 1);
            init_module(fd, CAPTURE_IX, brightness_cb, destroy_brightness);
        }
    }
}

static void brightness_cb(void) {
    if (!conf.single_capture_mode) {
        uint64_t t;
        read(main_p[CAPTURE_IX].fd, &t, sizeof(uint64_t));
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
        return set_timeout(2 * conf.timeout[state.time] * get_screen_dpms(), 0, main_p[CAPTURE_IX].fd, 0);
    }

    double drop = 0.0;

    double val = capture_frames_brightness();
    if (!state.quit) {
        if (val != -1) {
            INFO("Average frames brightness: %lf.\n", val);
            drop = set_brightness(val);
        }
    }

    if (!conf.single_capture_mode && !state.quit) {
        // if there is too high difference, do a fast recapture to be sure
        // this is the correct level
        if (fabs(drop) > drop_limit) {
            INFO("Weird brightness drop. Recapturing in 15 seconds.\n");
            // single call after 15s
            set_timeout(fast_timeout, 0, main_p[CAPTURE_IX].fd, 0);
        } else {
            // reset normal timer
            set_timeout(conf.timeout[state.time], 0, main_p[CAPTURE_IX].fd, 0);
        }
    }
}

static void get_max_brightness(void) {
    struct bus_args args = {"org.clightd.backlight", "/org/clightd/backlight", "org.clightd.backlight", "getmaxbrightness"};
    bus_call(&br.max, "i", &args, "s", conf.screen_path);
}

static void get_current_brightness(void) {
    struct bus_args args = {"org.clightd.backlight", "/org/clightd/backlight", "org.clightd.backlight", "getbrightness"};
    bus_call(&br.current, "i", &args, "s", conf.screen_path);
}

static double set_brightness(double perc) {
    struct bus_args args = {"org.clightd.backlight", "/org/clightd/backlight", "org.clightd.backlight", "setbrightness"};
    br.old = br.current;
    bus_call(&br.current, "i", &args, "si", conf.screen_path, (int) (br.max * perc));
    if (br.current != -1) {
        INFO("New brightness value: %d\n", br.current);
    }
    return (double)(br.current - br.old) / br.max;
}

static double capture_frames_brightness(void) {
    double brightness = 0.0;
    struct bus_args args = {"org.clightd.backlight", "/org/clightd/backlight", "org.clightd.backlight", "captureframes"};
    bus_call(&brightness, "d", &args, "si", conf.dev_name, conf.num_captures);
    return brightness;
}

void destroy_brightness(void) {
    if (main_p[CAPTURE_IX].fd > 0) {
        close(main_p[CAPTURE_IX].fd);
    }
}
