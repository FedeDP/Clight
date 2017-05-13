#include "../inc/brightness.h"
#include "../inc/dpms.h"
#include <gsl/gsl_multifit.h>

static void init(void);
static int check(void);
static void destroy(void);
static void brightness_cb(void);
static void do_capture(void);
static void get_max_brightness(void);
static void get_current_brightness(void);
static void set_brightness(const double perc);
static double capture_frames_brightness(void);
static void polynomialfit(void);
static double clamp(double value, double max, double min);

/*
 * Storage struct for our needed variables.
 */
struct brightness {
    int current;
    int max;
    int old;
};

static struct brightness br;
#ifdef DPMS_PRESENT
static struct dependency dependencies[] = { {HARD, BUS}, {SOFT, GAMMA}, {SOFT, UPOWER}, {SOFT, DPMS} };
#else
static struct dependency dependencies[] = { {HARD, BUS}, {SOFT, GAMMA}, {SOFT, UPOWER} };
#endif
static struct self_t self = {
    .name = "Brightness",
    .idx = BRIGHTNESS,
    .num_deps = SIZE(dependencies),
    .deps =  dependencies
};

void set_brightness_self(void) {
    modules[self.idx].self = &self;
    modules[self.idx].init = init;
    modules[self.idx].check = check;
    modules[self.idx].destroy = destroy;
    set_self_deps(&self);
}

/*
 * Init brightness values (max and current)
 */
static void init(void) {
    get_max_brightness();
    if (!state.quit) {
        /* Compute polynomial best-fit parameters */
        polynomialfit();
        int fd = start_timer(CLOCK_MONOTONIC, 0, 1);
        init_module(fd, self.idx, brightness_cb);
    }
}

static int check(void) {
    return 0; /* Skeleton function needed for modules interface */
}

static void destroy(void) {
    /* Skeleton function needed for modules interface */
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

/*
 * When timerfd timeout expires, check if we are in screen power_save mode,
 * otherwise start streaming on webcam and set BRIGHTNESS fd of pollfd struct to
 * webcam device fd. This way our main poll will get events (frames) from webcam device too.
 */
static void do_capture(void) {
    static const int fast_timeout = 15;
    static const double drop_limit = 0.6;
    
    /* reset fast recapture */
    state.fast_recapture = 0;

#ifdef DPMS_PRESENT
    /*
     * if screen is currently blanked thanks to dpms,
     * do not do anything. Set a long timeout and return.
     * Timeout will increase as screen power management goes deeper.
     */
    if (get_screen_dpms() > 0) {
        INFO("Screen is currently in power saving mode. Avoid changing brightness and setting a long timeout.\n");
        return set_timeout(2 * conf.timeout[state.ac_state][state.time] * get_screen_dpms(), 0, main_p[self.idx].fd, 0);
    }
#endif

    double val = capture_frames_brightness();
    /* 
     * if captureframes clightd method did not return any non-critical error (eg: eperm).
     * I won't check setbrightness too because if captureframes did not return any error,
     * it is very very unlikely that setbrightness would return some.
     */
    if (!state.quit && val >= 0.0) {
        set_brightness(val * 10);
        
        if (!conf.single_capture_mode && !state.quit) {
            double drop = (double)(br.current - br.old) / br.max;
            // if there is too high difference, do a fast recapture to be sure
            // this is the correct level
            if (fabs(drop) > drop_limit) {
                INFO("Weird brightness drop. Recapturing in 15 seconds.\n");
                // single call after 15s
                set_timeout(fast_timeout, 0, main_p[self.idx].fd, 0);
                state.fast_recapture = 1;
            } else {
                // reset normal timer
                set_timeout(conf.timeout[state.ac_state][state.time], 0, main_p[self.idx].fd, 0);
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

static void set_brightness(const double perc) {
    /* y = a0 + a1x + a2x^2 */
    const double b = state.fit_parameters[0] + state.fit_parameters[1] * perc + state.fit_parameters[2] * pow(perc, 2);
    /* Correctly honor conf.max_backlight_pct */
    int new_br =  (float)br.max / 100 * conf.max_backlight_pct[state.ac_state] * clamp(b, 1, 0);
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
        br.current = new_br;
        INFO("Brightness level was already %d.\n", new_br);
    }
}

static double capture_frames_brightness(void) {
    double brightness = -1;
    struct bus_args args = {"org.clightd.backlight", "/org/clightd/backlight", "org.clightd.backlight", "captureframes"};
    bus_call(&brightness, "d", &args, "si", conf.dev_name, conf.num_captures);
    DEBUG("Average frames brightness: %lf.\n", brightness);
    return brightness;
}

/*
 * Big thanks to https://rosettacode.org/wiki/Polynomial_regression#C 
 */
static void polynomialfit(void) {
    gsl_multifit_linear_workspace *ws;
    gsl_matrix *cov, *X;
    gsl_vector *y, *c;
    double chisq;
    int i, j;
    
    X = gsl_matrix_alloc(SIZE_POINTS, DEGREE);
    y = gsl_vector_alloc(SIZE_POINTS);
    c = gsl_vector_alloc(DEGREE);
    cov = gsl_matrix_alloc(DEGREE, DEGREE);
    
    for(i=0; i < SIZE_POINTS; i++) {
        for(j=0; j < DEGREE; j++) {
            gsl_matrix_set(X, i, j, pow(i, j));
        }
        gsl_vector_set(y, i, conf.regression_points[i]);
    }
    
    ws = gsl_multifit_linear_alloc(SIZE_POINTS, DEGREE);
    gsl_multifit_linear(X, y, c, cov, &chisq, ws);
    
    /* store results ... */
    for(i=0; i < DEGREE; i++) {
        state.fit_parameters[i] = gsl_vector_get(c, i);
    }
    DEBUG("y = %lf + %lfx + %lfx^2\n", state.fit_parameters[0], state.fit_parameters[1], state.fit_parameters[2]);
    
    gsl_multifit_linear_free(ws);
    gsl_matrix_free(X);
    gsl_matrix_free(cov);
    gsl_vector_free(y);
    gsl_vector_free(c);
}

static double clamp(double value, double max, double min) {
    if (value > max) { 
        return max; 
    }
    if (value < min) {
        return min;
    }
    return value;
}

