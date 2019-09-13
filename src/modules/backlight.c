#include "bus.h"
#include "my_math.h"

enum backlight_pause { UNPAUSED = 0, DISPLAY = 1, SENSOR = 2, AUTOCALIB = 4, INHIBIT = 8 };

static void receive_paused(const msg_t *const msg, const void* userdata);
static void init_kbd_backlight(void);
static int is_sensor_available(void);
static void do_capture(bool reset_timer);
static void set_new_backlight(const double perc);
static void set_backlight_level(const double pct, const int is_smooth, const double step, const int timeout);
static void set_keyboard_level(const double level);
static int capture_frames_brightness(void);
static void upower_callback(void);
static void interface_autocalib_callback(bool new_val);
static void interface_curve_callback(curve_upd *up);
static void interface_timeout_callback(timeout_upd *up);
static void dimmed_callback(void);
static void time_callback(int old_val, int is_event);
static int on_sensor_change(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int get_current_timeout(void);
static void on_inbhibit_update(void);
static void pause_mod(enum backlight_pause type);
static void resume_mod(enum backlight_pause type);

static int sensor_available;
static int max_kbd_backlight;
static int bl_fd;
static int paused;              // counter of how many sources are pausing BACKLIGHT (state.display_state, sensor_available, conf.no_auto_calib)
static sd_bus_slot *slot;

DECLARE_MSG(bl_msg, BL_UPD);
DECLARE_MSG(kbd_msg, KBD_BL_UPD);
DECLARE_MSG(amb_msg, AMBIENT_BR_UPD);
DECLARE_MSG(capture_req, CAPTURE_REQ);

MODULE("BACKLIGHT");

static void init(void) {
    capture_req.capture.reset_timer = true;
    
    /* Compute polynomial best-fit parameters */
    polynomialfit(ON_AC);
    polynomialfit(ON_BATTERY);

    M_SUB(UPOWER_UPD);
    M_SUB(DISPLAY_UPD);
    M_SUB(INHIBIT_UPD);
    M_SUB(DAYTIME_UPD);
    M_SUB(IN_EVENT_UPD);
    M_SUB(BL_TO_REQ);
    M_SUB(CAPTURE_REQ);
    M_SUB(CURVE_REQ);
    M_SUB(NO_AUTOCALIB_REQ);
    M_SUB(BL_REQ);
    M_SUB(KBD_BL_REQ);

    /* We do not fail if this fails */
    SYSBUS_ARG(args, CLIGHTD_SERVICE, "/org/clightd/clightd/Sensor", "org.clightd.clightd.Sensor", "Changed");
    add_match(&args, &slot, on_sensor_change);
    
    /* 
     * This only initializes kbd backlight, 
     * but it won't use it if it is disabled
     */
    init_kbd_backlight();

    sensor_available = is_sensor_available();
    
    bl_fd = start_timer(CLOCK_BOOTTIME, 0, 1);
    m_register_fd(bl_fd, false, NULL);
    if (!sensor_available) {
        pause_mod(SENSOR);
    }
    if (conf.no_auto_calib) {
        /*
         * If automatic calibration is disabled, we need to ensure to start
         * from a well known backlight level for DIMMER to correctly work.
         * Force 100% backlight level.
         *
         * Cannot publish a BL_REQ as BACKLIGHT get paused.
         */
        set_backlight_level(1.0, false, 0, 0);
        set_keyboard_level(0.0);
        pause_mod(AUTOCALIB);
    }
}

static bool check(void) {
    return true;
}

static bool evaluate(void) {
    return !conf.no_backlight && state.day_time != -1 && state.ac_state != -1;
}

static void destroy(void) {
    if (slot) {
        slot = sd_bus_slot_unref(slot);
    }
    if (bl_fd >= 0) {
        close(bl_fd);
    }
}

static void receive(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
    case FD_UPD:
        read_timer(msg->fd_msg->fd);
        M_PUB(&capture_req);
        break;
    case UPOWER_UPD:
        upower_callback();
        break;
    case DISPLAY_UPD:
        dimmed_callback();
        break;
    case IN_EVENT_UPD:
    case DAYTIME_UPD: {
        daytime_upd *up = (daytime_upd *)MSG_DATA();
        time_callback(up->old, MSG_TYPE() == IN_EVENT_UPD);
        break;
    }
    case INHIBIT_UPD:
        on_inbhibit_update();
        break;
    case BL_TO_REQ: {
        timeout_upd *up = (timeout_upd *)MSG_DATA();
        interface_timeout_callback(up);
        break;
    }
    case CAPTURE_REQ: {
        capture_upd *up = (capture_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            do_capture(up->reset_timer);
        }
        break;
    }
    case CURVE_REQ: {
        curve_upd *up = (curve_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            interface_curve_callback(up);
        }
        break;
    }
    case NO_AUTOCALIB_REQ: {
        calib_upd *up = (calib_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            interface_autocalib_callback(up->new);
        }
        break;
    }
    case BL_REQ: {
        bl_upd *up = (bl_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            set_backlight_level(up->new, up->smooth, up->step, up->timeout);
        }
        break;
    }
    case KBD_BL_REQ: {
        bl_upd *up = (bl_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            set_keyboard_level(up->new);
        }
        break;
    }
    default:
        break;
    }
}

static void receive_paused(const msg_t *const msg, UNUSED const void* userdata) {
    /* In paused state we have deregistered our fd, thus we can only receive PubSub messages */
    switch (MSG_TYPE()) {
    case DISPLAY_UPD:
        dimmed_callback();
        break;
    case INHIBIT_UPD:
        on_inbhibit_update();
        break;
    case CURVE_REQ: {
        curve_upd *up = (curve_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            interface_curve_callback(up);
        }
        break;
    }
    case CAPTURE_REQ: {
        capture_upd *up = (capture_upd *)MSG_DATA();
        /* In paused state check that we're not dimmed/dpms and sensor is available */
        if (VALIDATE_REQ(up) && !state.display_state && sensor_available) {
            do_capture(up->reset_timer);
        }
        break;
    }
    case NO_AUTOCALIB_REQ: {
        calib_upd *up = (calib_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            interface_autocalib_callback(up->new);
        }
        break;
    }
    case BL_REQ: {
        /* In paused state check that we're not dimmed/dpms */
        bl_upd *up = (bl_upd *)MSG_DATA();
        if (VALIDATE_REQ(up) && !state.display_state) {
            set_backlight_level(up->new, up->smooth, up->step, up->timeout);
        }
        break;
    }
    case KBD_BL_REQ: {
        /* In paused state check that we're not dimmed/dpms */
        bl_upd *up = (bl_upd *)MSG_DATA();
        if (VALIDATE_REQ(up) && !state.display_state) {
            set_keyboard_level(up->new);
        }
        break;
    }
    default:
        break;
    }
}

static void init_kbd_backlight(void) {
    SYSBUS_ARG(kbd_args, "org.freedesktop.UPower", "/org/freedesktop/UPower/KbdBacklight", "org.freedesktop.UPower.KbdBacklight", "GetMaxBrightness");
    int r = call(&max_kbd_backlight, "i", &kbd_args, NULL);
    if (r) {
        INFO("Keyboard backlight calibration unsupported.\n");
    } else {
        INFO("Keyboard backlight calibration enabled.\n");
    }
}

static int is_sensor_available(void) {
    int available = 0;
    SYSBUS_ARG(args, CLIGHTD_SERVICE, "/org/clightd/clightd/Sensor", "org.clightd.clightd.Sensor", "IsAvailable");

    int r = call(&available, "sb", &args, "s", conf.dev_name);
    return r == 0 && available;
}

static void do_capture(bool reset_timer) {
    if (!capture_frames_brightness()) {
        /* Account for screen-emitted brightness */
        state.ambient_br -= state.screen_comp;
        if (state.ambient_br > conf.shutter_threshold) {
            set_new_backlight(state.ambient_br * 10);
            if (state.screen_comp > 0.0) {
                INFO("Ambient brightness: %.3lf (%.3lf screen compensation) -> Backlight pct: %.3lf.\n", state.ambient_br, state.screen_comp, state.current_bl_pct);
            } else {
                INFO("Ambient brightness: %.3lf -> Backlight pct: %.3lf.\n", state.ambient_br, state.current_bl_pct);
            }
        } else {
            INFO("Ambient brightness: %.3lf. Clogged capture detected.\n", state.ambient_br);
        }
    }

    if (reset_timer) {
        set_timeout(get_current_timeout(), 0, bl_fd, 0);
    }
}

static void set_new_backlight(const double perc) {
    /* y = a0 + a1x + a2x^2 */
    const double b = state.fit_parameters[state.ac_state][0] + state.fit_parameters[state.ac_state][1] * perc + state.fit_parameters[state.ac_state][2] * pow(perc, 2);
    const double new_br_pct =  clamp(b, 1, 0);

    set_backlight_level(new_br_pct, !conf.no_smooth_backlight, conf.backlight_trans_step, conf.backlight_trans_timeout);
    
    /*
     * keyboard backlight follows opposite curve:
     * on high ambient brightness, it must be very low (off)
     * on low ambient brightness, it must be turned on
     */
    set_keyboard_level(1.0 - new_br_pct);
}

static void set_keyboard_level(const double level) {
    if (max_kbd_backlight > 0 && !conf.no_keyboard_bl) {
        SYSBUS_ARG(kbd_args, "org.freedesktop.UPower", "/org/freedesktop/UPower/KbdBacklight", "org.freedesktop.UPower.KbdBacklight", "SetBrightness");

        kbd_msg.bl.old = state.current_kbd_pct;
        /* We actually need to pass an int to variadic bus() call */
        const int new_kbd_br = round(level * max_kbd_backlight);
        if (call(NULL, NULL, &kbd_args, "i", new_kbd_br) == 0) {
            state.current_kbd_pct = level;
            kbd_msg.bl.new = state.current_kbd_pct;
            M_PUB(&kbd_msg);
        }
    }
}

static void set_backlight_level(const double pct, const int is_smooth, const double step, const int timeout) {
    SYSBUS_ARG(args, CLIGHTD_SERVICE, "/org/clightd/clightd/Backlight", "org.clightd.clightd.Backlight", "SetAll");

    /* Set backlight on both internal monitor (in case of laptop) and external ones */
    int ok;
    int r = call(&ok, "b", &args, "d(bdu)s", pct, is_smooth, step, timeout, conf.screen_path);
    if (!r && ok) {
        bl_msg.bl.old = state.current_bl_pct;
        state.current_bl_pct = pct;
        bl_msg.bl.new = pct;
        bl_msg.bl.smooth = is_smooth;
        bl_msg.bl.step = step;
        bl_msg.bl.timeout = timeout;
        M_PUB(&bl_msg);
    }
}

static int capture_frames_brightness(void) {
    SYSBUS_ARG(args, CLIGHTD_SERVICE, "/org/clightd/clightd/Sensor", "org.clightd.clightd.Sensor", "Capture");
    double intensity[conf.num_captures];
    int r = call(intensity, "sad", &args, "sis", conf.dev_name, conf.num_captures, conf.dev_opts);
    if (!r) {
        amb_msg.bl.old = state.ambient_br;
        state.ambient_br = compute_average(intensity, conf.num_captures);
        DEBUG("Average frames brightness: %lf.\n", state.ambient_br);
        amb_msg.bl.new = state.ambient_br;
        M_PUB(&amb_msg);
    }
    return r;
}

/* Callback on upower ac state changed signal */
static void upower_callback(void) {
    set_timeout(0, 1, bl_fd, 0);
}

/* Callback on "NoAutoCalib" bus exposed writable property */
static void interface_autocalib_callback(bool new_val) {
    INFO("Backlight autocalibration %s.\n", new_val ? "disabled" : "enabled");
    conf.no_auto_calib = new_val;
    if (conf.no_auto_calib) {
        pause_mod(AUTOCALIB);
    } else {
        resume_mod(AUTOCALIB);
    }
}

/* Callback on "AcCurvePoints" and "BattCurvePoints" bus exposed writable properties */
static void interface_curve_callback(curve_upd *up) {
    memcpy(conf.regression_points[up->state], up->regression_points, up->num_points * sizeof(double));
    conf.num_points[up->state] = up->num_points;
    polynomialfit(up->state);
}

/* Callback on "backlight_timeout" bus exposed writable properties */
static void interface_timeout_callback(timeout_upd *up) {
    if (up->state >= ON_AC && up->state < SIZE_AC && up->daytime >= DAY && up->daytime < SIZE_STATES) {
        const int old = get_current_timeout();
        conf.timeout[up->state][up->daytime] = up->new;
        if (up->state == state.ac_state && (up->daytime == state.day_time || (state.in_event && up->daytime == IN_EVENT))) {
            reset_timer(bl_fd, old, get_current_timeout());
        }
    } else {
        WARN("Failed to validate timeout request.\n");
    }
}

/* Callback on state.display_state changes */
static void dimmed_callback(void) {
    if (state.display_state) {
        pause_mod(DISPLAY);
    } else {
        resume_mod(DISPLAY);
    }
}

/* Callback on state.time/state.in_event changes */
static void time_callback(int old_val, int is_event) {
    int old_timeout;
    if (!is_event) {
        /* A state.time change happened, react! */
        old_timeout = conf.timeout[state.ac_state][old_val];
    } else {
        /* A state.in_event change happened, react!
         * If state.in_event is now true, it means we were in state.time timeout.
         * Else, an event ended, thus we were IN_EVENT.
         */
        old_timeout = conf.timeout[state.ac_state][state.in_event ? state.day_time : IN_EVENT];
    }
    reset_timer(bl_fd, old_timeout, get_current_timeout());
}

/* Callback on SensorChanged clightd signal */
static int on_sensor_change(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int new_sensor_avail = is_sensor_available();
    if (new_sensor_avail != sensor_available) {
        sensor_available = new_sensor_avail;
        if (sensor_available) {
            DEBUG("Resumed as a sensor is now available.\n");
            resume_mod(SENSOR);
        } else {
            DEBUG("Paused as no sensor is available.\n");
            pause_mod(SENSOR);
        }
    }
    return 0;
}

static inline int get_current_timeout(void) {
    if (state.in_event) {
        return conf.timeout[state.ac_state][IN_EVENT];
    }
    return conf.timeout[state.ac_state][state.day_time];
}

static void on_inbhibit_update(void) {
    if (conf.inhibit_autocalib && state.inhibited) {
        pause_mod(INHIBIT);
    } else {
        /* 
         * Always resume: this is needed
         * in case INTERFACE disables conf.inhibit_autocalib
         * while we are inhibited.
         * Note that it won't do anything if we were not inhibited.
         */
        resume_mod(INHIBIT);
    }
}

static void pause_mod(enum backlight_pause type) {
    int old_paused = paused;
    paused |= type;
    if (old_paused == UNPAUSED && paused != UNPAUSED) {
        m_become(paused);
        /* Properly deregister our fd while paused */
        m_deregister_fd(bl_fd);
    }
}

static void resume_mod(enum backlight_pause type) {
    int old_paused = paused;
    paused &= ~type;
    if (old_paused != UNPAUSED && paused == UNPAUSED) {
        m_unbecome();
        /* Register back our fd on resume */
        m_register_fd(bl_fd, false, NULL);
    }
}
