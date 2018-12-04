#include <backlight.h>
#include <bus.h>
#include <my_math.h>
#include <interface.h>

static void init_kbd_backlight(void);
static int is_sensor_available(void);
static void do_capture(int reset_timer);
static void set_new_backlight(const double perc);
static void set_keyboard_level(const double level);
static int capture_frames_brightness(void);
static void upower_callback(const void *ptr);
static void interface_calibrate_callback(const void *ptr);
static void interface_curve_callback(const void *ptr);
static void interface_timeout_callback(const void *ptr);
static void dimmed_callback(void);
static void time_callback(void);
static int on_sensor_change(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

static int sensor_available;
static int ac_force_capture;
static int max_kbd_backlight;
static sd_bus_slot *slot;
static struct dependency dependencies[] = { 
    {SOFT, GAMMA},      // Which time of day are we in?
    {SOFT, UPOWER},     // Are we on AC or on BATT?
    {HARD, CLIGHTD},    // methods to set screen backlight
    {HARD, INTERFACE}   // We need INTERFACE module because we are subscribed to "Dimmed" and "Time" signals (thus HARD dep) + we add callbacks on INTERFACE
}; 
static struct self_t self = {
    .num_deps = SIZE(dependencies),
    .deps =  dependencies,
    .functional_module = 1
};

MODULE(BACKLIGHT);

static void init(void) {
    struct bus_cb upower_cb = { UPOWER, upower_callback };
    struct bus_cb interface_calibrate_cb = { INTERFACE, interface_calibrate_callback, "calibrate" };
    struct bus_cb interface_curve_cb = { INTERFACE, interface_curve_callback, "curve" };
    struct bus_cb interface_to_cb = { INTERFACE, interface_timeout_callback, "backlight_timeout" };

    /* Compute polynomial best-fit parameters */
    polynomialfit(ON_AC);
    polynomialfit(ON_BATTERY);

    /* Add callbacks on prop signal emitted by interface module */
    struct prop_cb dimmed_cb = { "Dimmed", dimmed_callback };
    struct prop_cb time_cb = { "Time", time_callback };
    add_prop_callback(&dimmed_cb);
    add_prop_callback(&time_cb);

    /* We do not fail if this fails */
    SYSBUS_ARG(args, CLIGHTD_SERVICE, "/org/clightd/clightd/Sensor", "org.clightd.clightd.Sensor", "Changed");
    add_match(&args, &slot, on_sensor_change);

    if (!conf.no_keyboard_bl) {
        init_kbd_backlight();
    }

    /* Start module timer: 1ns delay if sensor is available, else start it paused */
    sensor_available = is_sensor_available();
    int fd = start_timer(CLOCK_BOOTTIME, 0, sensor_available);

    INIT_MOD(fd, &upower_cb, &interface_calibrate_cb, &interface_curve_cb, &interface_to_cb);
}

static int check(void) {
    return 0;
}

static void destroy(void) {
    if (slot) {
        slot = sd_bus_slot_unref(slot);
    }
}

static void callback(void) {
    uint64_t t;
    read(main_p[self.idx].fd, &t, sizeof(uint64_t));

    do_capture(1);
}

static void init_kbd_backlight(void) {
    SYSBUS_ARG(kbd_args, "org.freedesktop.UPower", "/org/freedesktop/UPower/KbdBacklight", "org.freedesktop.UPower.KbdBacklight", "GetMaxBrightness");
    int r = call(&max_kbd_backlight, "i", &kbd_args, NULL);
    if (r) {
        INFO("Keyboard backlight calibration unsupported.\n");
    }
}

static int is_sensor_available(void) {
    int available = 0;
    SYSBUS_ARG(args, CLIGHTD_SERVICE, "/org/clightd/clightd/Sensor", "org.clightd.clightd.Sensor", "IsAvailable");

    int r = call(&available, "sb", &args, "s", conf.dev_name);
    return r == 0 && available;
}

static void do_capture(int reset_timer) {
    if (!capture_frames_brightness()) {
        if (state.ambient_br > conf.shutter_threshold) {
            set_new_backlight(state.ambient_br * 10);
            INFO("Ambient brightness: %.3lf -> Backlight pct: %.3lf\n", state.ambient_br, state.current_bl_pct);
        } else {
            INFO("Ambient brightness: %.3lf. Clogged capture detected.\n", state.ambient_br);
        }
    }

    if (reset_timer) {
        set_timeout(conf.timeout[state.ac_state][state.time], 0, main_p[self.idx].fd, 0);
    }
}

static void set_new_backlight(const double perc) {
    /* y = a0 + a1x + a2x^2 */
    const double b = state.fit_parameters[state.ac_state][0] + state.fit_parameters[state.ac_state][1] * perc + state.fit_parameters[state.ac_state][2] * pow(perc, 2);
    const double new_br_pct =  clamp(b, 1, 0);

    set_backlight_level(new_br_pct, !conf.no_smooth_backlight, conf.backlight_trans_step, conf.backlight_trans_timeout);
    set_keyboard_level(new_br_pct);
}

static void set_keyboard_level(const double level) {
    if (max_kbd_backlight > 0) {
        SYSBUS_ARG(kbd_args, "org.freedesktop.UPower", "/org/freedesktop/UPower/KbdBacklight", "org.freedesktop.UPower.KbdBacklight", "SetBrightness");
        /* 
         * keyboard backlight follows opposite curve: 
         * on high ambient brightness, it must be very low (off)
         * on low ambient brightness, it must be turned on
         */
        int kbd_pct = (1.0 - level) * max_kbd_backlight; 
        call(NULL, NULL, &kbd_args, "i", kbd_pct);
    }
}

void set_backlight_level(const double pct, const int is_smooth, const double step, const int timeout) {
    SYSBUS_ARG(args, CLIGHTD_SERVICE, "/org/clightd/clightd/Backlight", "org.clightd.clightd.Backlight", "SetAll");

    /* Set backlight on both internal monitor (in case of laptop) and external ones */
    int ok;
    int r = call(&ok, "b", &args, "d(bdu)s", pct, is_smooth, step, timeout, conf.screen_path);
    if (!r && ok) {
        state.current_bl_pct = pct;
        emit_prop("CurrentBlPct");
    }
}

static int capture_frames_brightness(void) {
    SYSBUS_ARG(args, CLIGHTD_SERVICE, "/org/clightd/clightd/Sensor", "org.clightd.clightd.Sensor", "Capture");
    double intensity[conf.num_captures];
    int r = call(intensity, "sad", &args, "si", conf.dev_name, conf.num_captures);
    if (!r) {
        state.ambient_br = compute_average(intensity, conf.num_captures);
        emit_prop("CurrentAmbientBr");
    }
    return r;
}

/* Callback on upower ac state changed signal */
static void upower_callback(const void *ptr) {
    int old_ac_state = *(int *)ptr;
    /* Force check that we received an ac_state changed event for real */
    if (old_ac_state != state.ac_state && sensor_available) {
        if (!state.is_dimmed) {
            /* 
            * do a capture right now as we have 2 different curves for 
            * different AC_STATES, so let's properly honor new curve
            */
            set_timeout(0, 1, main_p[self.idx].fd, 0);
        }  else {
            // if it is even, it means eg: we started on AC, then on Batt, then again on AC (so, ac state is not changed at all in the end while we where dimmed)
            ac_force_capture++;
        }
    }
}

/* Callback on "Calibrate" bus interface method */
static void interface_calibrate_callback(const void *ptr) {
    if (!state.is_dimmed && sensor_available) {
        do_capture(0);
    }
}

/* Callback on "AcCurvePoints" and "BattCurvePoints" bus exposed writable properties */
static void interface_curve_callback(const void *ptr) {
    enum ac_states s = *((int *)ptr);
    polynomialfit(s);
}

/* Callback on "backlight_timeout" bus exposed writable properties */
static void interface_timeout_callback(const void *ptr) {
    if (!state.is_dimmed && sensor_available) {
        int old_val = *((int *)ptr);
        reset_timer(main_p[self.idx].fd, old_val, conf.timeout[state.ac_state][state.time]);
    }
}

/* Callback on state.is_dimmed changes */
static void dimmed_callback(void) {
    static int old_sensor_available = 1;

    if (sensor_available) {
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
                // Immediately force a capture if ac state changed while we were dimmed 
                timeout_nsec = 1;
            } else if (sensor_available != old_sensor_available) {
                // Immediately force a capture if a sensor appeared while we were dimmed 
                timeout_nsec = 1;
            } else {
                // Apply correct timeout for current ac state and day time
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
    old_sensor_available = sensor_available;
}

/* Callback on state.time changes */
static void time_callback(void) {
    static enum states old_state = DAY; // default initial value
    if (!state.is_dimmed && sensor_available) {
        /* A state.time change happened, react! */
        reset_timer(main_p[self.idx].fd, conf.timeout[state.ac_state][old_state], conf.timeout[state.ac_state][state.time]);
    }
    old_state = state.time;
}

/* Callback on SensorChanged clightd signal */
static int on_sensor_change(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int new_sensor_avail = is_sensor_available();
    if (new_sensor_avail != sensor_available) {
        if (!state.is_dimmed) {
            // Resume module if sensor is now available. Pause it if it is not available
            set_timeout(0, new_sensor_avail, main_p[self.idx].fd, 0);
        }
        sensor_available = new_sensor_avail;
        if (sensor_available) {
            INFO("%s module resumed as a sensor is available.\n", self.name);
        } else {
            INFO("%s module paused as no sensor is available.\n", self.name);
        }
    }
    return 0;
}
