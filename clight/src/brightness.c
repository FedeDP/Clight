#include "../inc/brightness.h"
#include "../inc/dpms.h"

static void brightness_cb(void);
static void do_capture(void);
static void get_max_brightness(void);
static void get_current_brightness(void);

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

        /* Initialize our brightness values array */
        if (!state.quit && !(state.values = calloc(conf.num_captures, sizeof(double)))) {
            state.quit = 1;
        } else {
            int fd = start_timer(CLOCK_MONOTONIC, 1);
            set_pollfd(fd, CAPTURE_IX, brightness_cb);
            INFO("Brightness module started.\n");
        }
    }
}

static void brightness_cb(void) {
    if (!conf.single_capture_mode) {
        uint64_t t;

        read(main_p.p[CAPTURE_IX].fd, &t, sizeof(uint64_t));
    }
    do_capture();
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
        return set_timeout(2 * conf.timeout[state.time] * get_screen_dpms(), 0, main_p.p[CAPTURE_IX].fd, 0);
    }

    double drop = 0.0;

    for (int i = 0; i < conf.num_captures && !state.quit; i++) {
        state.values[i] = capture_frame();
    }

    if (!state.quit) {
        double new_val = compute_avg_brightness();
        if (new_val != 0.0) {
            INFO("Average frames brightness: %lf.\n", new_val);
            drop = set_brightness(new_val);
        }
    }

    if (!conf.single_capture_mode && !state.quit) {
        // if there is too high difference, do a fast recapture to be sure
        // this is the correct level
        if (fabs(drop) > drop_limit) {
            INFO("Weird brightness drop. Recapturing in 15 seconds.\n");
            // single call after 15s
            set_timeout(fast_timeout, 0, main_p.p[CAPTURE_IX].fd, 0);
        } else {
            // reset normal timer
            set_timeout(conf.timeout[state.time], 0, main_p.p[CAPTURE_IX].fd, 0);
        }
    }
}

static void get_max_brightness(void) {
    struct bus_args args = {"org.clight.backlight", "/org/clight/backlight", "org.clight.backlight", "getmaxbrightness"};

    bus_call(&br.max, "i", &args, "s", conf.screen_path);
}

static void get_current_brightness(void) {
    struct bus_args args = {"org.clight.backlight", "/org/clight/backlight", "org.clight.backlight", "getbrightness"};

    bus_call(&br.current, "i", &args, "s", conf.screen_path);
}

double set_brightness(double perc) {
    struct bus_args args = {"org.clight.backlight", "/org/clight/backlight", "org.clight.backlight", "setbrightness"};

    bus_call(&br.current, "i", &args, "si", conf.screen_path, (int) (br.max * perc));
    if (br.current != -1) {
        INFO("New brightness value: %d\n", br.current);
    }
    return (double)(br.current - br.old) / br.max;
}

double capture_frame(void) {
    double brightness = 0.0;
    struct bus_args args = {"org.clight.backlight", "/org/clight/backlight", "org.clight.backlight", "captureframe"};

    bus_call(&brightness, "d", &args, "s", conf.dev_name);
    return brightness;
}

/*
 * Compute average captured frames brightness.
 * It will normalize data removing highest and lowest values.
 */
double compute_avg_brightness(void) {
    int lowest = 0, highest = 0;
    double total = 0.0;

    for (int i = 0; i < conf.num_captures; i++) {
        if (state.values[i] < state.values[lowest]) {
            lowest = i;
        } else if (state.values[i] > state.values[highest]) {
            highest = i;
        }

        total += state.values[i];
    }

    // total == 0.0 means every captured frame decompression failed
    if (total != 0.0 && conf.num_captures > 2) {
        // remove highest and lowest values to normalize
        total -= (state.values[highest] + state.values[lowest]);
        return total / (conf.num_captures - 2);
    }

    return total / conf.num_captures;
}

void destroy_brightness(void) {
    if (state.values) {
        free(state.values);
    }

    if (main_p.p[CAPTURE_IX].fd != -1) {
        close(main_p.p[CAPTURE_IX].fd);
    }
    INFO("Brightness module destroyed.\n");
}
