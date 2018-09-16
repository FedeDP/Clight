#include <brightness.h>
#include <bus.h>
#include <my_math.h>
#include <interface.h>

static void do_capture(int reset_timer);
static void set_brightness(const double perc);
static double capture_frames_brightness(void);
static void upower_callback(const void *ptr);
static void interface_callback(const void *ptr);
static void dimmed_callback(void);
static void time_callback(void);

static sd_bus_slot *slot;
static struct dependency dependencies[] = { {SOFT, GAMMA}, {SOFT, UPOWER}, {HARD, CLIGHTD}, {HARD, INTERFACE} };
static struct self_t self = {
    .num_deps = SIZE(dependencies),
    .deps =  dependencies,
    .functional_module = 1
};
static int ac_force_capture;

MODULE(BRIGHTNESS);

/*
 * Init brightness values (max and current)
 */
static void init(void) {
    struct bus_cb upower_cb = { UPOWER, upower_callback };
    struct bus_cb interface_cb = { INTERFACE, interface_callback, "calibrate" };
    
    /* Compute polynomial best-fit parameters */
    for (int i = 0; i < SIZE_AC; i++) {
        polynomialfit(i, conf.regression_points[i]);
    }

    /* Add callbacks on prop signal emitted by interface module */
    struct prop_cb dimmed_cb = { "Dimmed", dimmed_callback };
    struct prop_cb time_cb = { "Time", time_callback };
    add_prop_callback(&dimmed_cb);
    add_prop_callback(&time_cb);
    
    /* Start module timer */
    int fd = start_timer(CLOCK_BOOTTIME, 0, 1);
    INIT_MOD(fd, &upower_cb, &interface_cb);
}

static int check(void) {
    int webcam_available;
    struct bus_args args = {"org.clightd.backlight", "/org/clightd/backlight", "org.clightd.backlight", "iswebcamavailable"};
    
    int r = call(&webcam_available, "b", &args, NULL);
    return r || !webcam_available;
}

static void destroy(void) {
    /* Destroy this match slot */
    if (slot) {
        slot = sd_bus_slot_unref(slot);
    }
}

static void callback(void) {
    uint64_t t;
    read(main_p[self.idx].fd, &t, sizeof(uint64_t));
    
    do_capture(1);
}

/*
 * When timerfd timeout expires, check if we are in screen power_save mode,
 * otherwise start streaming on webcam and set BRIGHTNESS fd of pollfd struct to
 * webcam device fd. This way our main poll will get events (frames) from webcam device too.
 */
static void do_capture(int reset_timer) {
    double val = capture_frames_brightness();
    /* 
     * if captureframes clightd method did not return any non-critical error (eg: eperm).
     * I won't check setbrightness too because if captureframes did not return any error,
     * it is very very unlikely that setbrightness would return some.
     */
    if (val >= 0.0) {
        set_brightness(val * 10);
    }
    
    if (reset_timer) {
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
        emit_prop("CurrentBrPct");
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

/* Callback on upower ac state changed signal */
static void upower_callback(const void *ptr) {
    int old_ac_state = *(int *)ptr;
    /* Force check that we received an ac_state changed event for real */
    if (old_ac_state != state.ac_state) {
        if (!state.is_dimmed) {
            /* 
            * do a capture right now as we have 2 different curves for 
            * different AC_STATES, so let's properly honor new curve
            */
            set_timeout(0, 1, main_p[self.idx].fd, 0);
        }  else {
            ac_force_capture++; // if it is even, it means eg: we started on AC, then on Batt, then again on AC (so, ac state is not changed at all in the end while we where dimmed)
        }
    }
}

/* Callback on "Calibrate" bus interface method */
static void interface_callback(const void *ptr) {
    if (!state.is_dimmed) {
        do_capture(0);
    }
}

/* Callback on state.is_dimmed changes */
static void dimmed_callback(void) {
    static int old_elapsed = 0;
    
    if (state.is_dimmed && old_elapsed == 0) {
        old_elapsed = conf.timeout[state.ac_state][state.time] - get_timeout_sec(main_p[self.idx].fd);
        set_timeout(0, 0, main_p[self.idx].fd, 0); // pause ourself
    } else {
        /* 
         * We have just left dimmed state, reset old timeout 
         * (checking if ac state changed in the meantime)
         */
        int timeout_nsec = 0;
        int timeout_sec = 0;
        if ((ac_force_capture % 2) == 1) {
            timeout_nsec = 1;
        } else {
            timeout_sec = conf.timeout[state.ac_state][state.time] - old_elapsed;
            if (timeout_sec <= 0) {
                timeout_nsec = 1;
            }
        }
        set_timeout(timeout_sec, timeout_nsec, main_p[self.idx].fd, 0);
        old_elapsed = 0;
        ac_force_capture = 0;
    }
}

/* Callback on state.time changes */
static void time_callback(void) {
    static enum states old_state = DAY; // default initial value
    if (!state.is_dimmed) {
        /* A state.time change happened, react! */
        reset_timer(main_p[self.idx].fd, conf.timeout[state.ac_state][old_state], conf.timeout[state.ac_state][state.time]);
    }
    old_state = state.time;
}
