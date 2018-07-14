#include "../inc/brightness.h"
#include "../inc/bus.h"
#include "../inc/math_utils.h"

static void init(void);
static int check(void);
static void destroy(void);
static void callback(void);
static void do_capture(void);
static void set_brightness(const double perc);
static double capture_frames_brightness(void);
static void upower_callback(const void *ptr);

static struct dependency dependencies[] = { {HARD, BUS}, {SOFT, GAMMA}, {SOFT, UPOWER}, {HARD, CLIGHTD} };
static struct self_t self = {
    .name = "Brightness",
    .idx = BRIGHTNESS,
    .num_deps = SIZE(dependencies),
    .deps =  dependencies,
    .standalone = 1,
    .enabled_single_capture = 1,
    .functional_module = 1
};

void set_brightness_self(void) {
    SET_SELF();
}

/*
 * Init brightness values (max and current)
 */
static void init(void) {
    /* Compute polynomial best-fit parameters */
    polynomialfit(ON_AC);
    polynomialfit(ON_BATTERY);
    /* Assume max backlight level on startup */
    state.current_br_pct = 1.0;
    int fd = start_timer(CLOCK_BOOTTIME, 0, 1);
    
    struct bus_cb upower_cb = { UPOWER, upower_callback };
    INIT_MOD(fd, &upower_cb);
}

static int check(void) {
    int webcam_available;
    struct bus_args args = {"org.clightd.backlight", "/org/clightd/backlight", "org.clightd.backlight", "iswebcamavailable"};
    
    int r = call(&webcam_available, "b", &args, NULL);
    return r || !webcam_available;
}

static void destroy(void) {
    /* Skeleton function needed for modules interface */
}

static void callback(void) {
    uint64_t t;
    read(main_p[self.idx].fd, &t, sizeof(uint64_t));
    
    do_capture();
    if (conf.single_capture_mode) {
        state.quit = NORM_QUIT;
    }
}

/*
 * When timerfd timeout expires, check if we are in screen power_save mode,
 * otherwise start streaming on webcam and set BRIGHTNESS fd of pollfd struct to
 * webcam device fd. This way our main poll will get events (frames) from webcam device too.
 */
static void do_capture(void) {
    /* reset fast recapture */
    state.fast_recapture = 0;

    if (!state.is_dimmed) {
        double val = capture_frames_brightness();
        /* 
         * if captureframes clightd method did not return any non-critical error (eg: eperm).
         * I won't check setbrightness too because if captureframes did not return any error,
         * it is very very unlikely that setbrightness would return some.
         */
        if (val >= 0.0) {
            set_brightness(val * 10);
        }
    } else {
        DEBUG("Screen is currently dimmed. Avoid changing backlight level.\n");
    }

    if (!conf.single_capture_mode) {
        // reset normal timer
        set_timeout(conf.timeout[state.ac_state][state.time], 0, main_p[self.idx].fd, 0);
    }
}

static void set_brightness(const double perc) {
    /* y = a0 + a1x + a2x^2 */
    const double b = state.fit_parameters[state.ac_state][0] + state.fit_parameters[state.ac_state][1] * perc + state.fit_parameters[state.ac_state][2] * pow(perc, 2);
    const double new_br_pct =  clamp(b, 1, 0);
    
    INFO("New brightness pct value: %f\n", new_br_pct);
    set_backlight_level(new_br_pct, !conf.no_smooth_backlight, conf.backlight_trans_step, conf.backlight_trans_timeout);
}

void set_backlight_level(const double pct, const int is_smooth, const double step, const int timeout) {
    struct bus_args args = {"org.clightd.backlight", "/org/clightd/backlight", "org.clightd.backlight", "setallbrightness"};
    
    /* Set brightness on both internal monitor (in case of laptop) and external ones */
    int ok;
    int r = call(&ok, "b", &args, "d(bdu)s", pct, is_smooth, step, timeout, conf.screen_path);
    if (!r && ok) {
        state.current_br_pct = pct;
    }
}

static double capture_frames_brightness(void) {
    struct bus_args args = {"org.clightd.backlight", "/org/clightd/backlight", "org.clightd.backlight", "captureframes"};
    double intensity[conf.num_captures];
    int r = call(intensity, "ad", &args, "si", conf.dev_name, conf.num_captures);
    if (!r) {
        return compute_average(intensity, conf.num_captures);
    }
    return -1.0f;
}

static void upower_callback(const void *ptr) {
    int old_ac_state = *(int *)ptr;
    /* Force check that we received an ac_state changed event for real */
    if (!state.fast_recapture && old_ac_state != state.ac_state) {
        /* 
         * do a capture right now as we have 2 different curves for 
         * different AC_STATES, so let's properly honor new curve
         */
        set_timeout(0, 1, main_p[self.idx].fd, 0);
    }
}
