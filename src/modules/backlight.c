#include <backlight.h>
#include <bus.h>
#include <my_math.h>
#include <interface.h>

static void receive_paused(const msg_t *const msg, const void* userdata);
static void init_kbd_backlight(void);
static int is_sensor_available(void);
static void do_capture(int reset_timer);
static void set_new_backlight(const double perc);
static void set_keyboard_level(const double level);
static int capture_frames_brightness(void);
static void upower_callback(void);
static void interface_calibrate_callback(void);
static void interface_autocalib_callback(void);
static void interface_curve_callback(enum ac_states s);
static void interface_timeout_callback(int old_val);
static void dimmed_callback(void);
static void time_callback(void);
static int on_sensor_change(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int get_current_timeout(void);
static void pause_mod(void);
static void resume_mod(void);

static int curr_timeout;
static int sensor_available;
static int max_kbd_backlight;
static int bl_fd;
static sd_bus_slot *slot;

static bl_upd bl_msg = { CURRENT_BL };
static bl_upd kbd_msg = { CURRENT_KBD_BL };
static bl_upd amb_msg = { AMBIENT_BR };
static state_upd pause_msg = { PAUSE_UPD };
static state_upd resume_msg = { RESUME_UPD };

const char *current_bl_topic = "CurrentBlPct";
const char *current_kbd_topic = "CurrentKbdPct";
const char *current_ab_topic = "CurrentAmbientBr"; 

MODULE("BACKLIGHT");

/**
 * FIXME:
 * * FIX pause_mod()/resume_mod()
 * * Drop curr_timeout
 * * Drop sensor_available
 **/

static void init(void) {
    /* Compute polynomial best-fit parameters */
    polynomialfit(ON_AC);
    polynomialfit(ON_BATTERY);

    m_subscribe(up_topic);
    m_subscribe(display_topic);
    m_subscribe(time_topic);
    m_subscribe(evt_topic);
    m_subscribe(interface_bl_to_topic);
    m_subscribe(interface_bl_capture);
    m_subscribe(interface_bl_curve);
    m_subscribe(interface_bl_autocalib);

    /* We do not fail if this fails */
    SYSBUS_ARG(args, CLIGHTD_SERVICE, "/org/clightd/clightd/Sensor", "org.clightd.clightd.Sensor", "Changed");
    add_match(&args, &slot, on_sensor_change);
    
    /* 
     * This only initializes kbd backlight, 
     * but it won't use it if it is disabled
     */
    init_kbd_backlight();

    curr_timeout = get_current_timeout();
    
    /* Start module timer: 1ns delay if sensor is available and autocalib is enabled, else start it paused */
    sensor_available = is_sensor_available();
    /* When no_auto_calib is true, start paused */
    bl_fd = start_timer(CLOCK_BOOTTIME, 0, sensor_available && !conf.no_auto_calib);
    m_register_fd(bl_fd, false, NULL);
}

static bool check(void) {
    return true;
}

static bool evaluate(void) {
    return conf.no_backlight == 0 && (conf.no_gamma || state.time != -1);
}

static void destroy(void) {
    if (slot) {
        slot = sd_bus_slot_unref(slot);
    }
    if (bl_fd >= 0) {
        close(bl_fd);
    }
}

static void receive(const msg_t *const msg, const void* userdata) {
    if (!msg->is_pubsub) {
        uint64_t t;
        read(msg->fd_msg->fd, &t, sizeof(uint64_t));
        do_capture(1);
    } else if (msg->ps_msg->type == USER) {
        MSG_TYPE();
        switch (type) {
            case UPOWER_UPDATE:
                upower_callback();
                break;
            case DISPLAY_UPDATE:
                dimmed_callback();
                break;
            case TIME_UPDATE:
                time_callback();
                break;
            case TIMEOUT_UPDATE: {
                timeout_upd *up = (timeout_upd *)msg->ps_msg->message;
                interface_timeout_callback(up->old);
                }
                break;
            case DO_CAPTURE:
                interface_calibrate_callback();
                break;
            case CURVE_UPDATE: {
                curve_upd *up = (curve_upd *)msg->ps_msg->message;
                interface_curve_callback(up->state);
                }
                break;
            case AUTOCALIB_UPD:
                interface_autocalib_callback();
                break;
            case PAUSE_UPD:                
                /* Properly deregister our fd while paused */
                m_deregister_fd(bl_fd);
                break;
            case RESUME_UPD:
                m_register_fd(bl_fd, false, NULL);
                break;
            default:
                break;
        }
    }
}

static void receive_paused(const msg_t *const msg, const void* userdata) {
    if (msg->ps_msg->type == USER) {
        MSG_TYPE();
        switch (type) {
            case DISPLAY_UPDATE:
                dimmed_callback();
                break;
            case CURVE_UPDATE: {
                curve_upd *up = (curve_upd *)msg->ps_msg->message;
                interface_curve_callback(up->state);
            }
            break;
            case DO_CAPTURE:
                interface_calibrate_callback();
                break;
            case AUTOCALIB_UPD:
                interface_autocalib_callback();
                break;
            default:
                break;
        }
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
        set_timeout(curr_timeout, 0, bl_fd, 0);
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
    if (max_kbd_backlight > 0 && !conf.no_keyboard_bl) {
        SYSBUS_ARG(kbd_args, "org.freedesktop.UPower", "/org/freedesktop/UPower/KbdBacklight", "org.freedesktop.UPower.KbdBacklight", "SetBrightness");
        /*
         * keyboard backlight follows opposite curve:
         * on high ambient brightness, it must be very low (off)
         * on low ambient brightness, it must be turned on
         */
        state.current_kbd_pct = 1.0 - level;
        if (call(NULL, NULL, &kbd_args, "i", state.current_kbd_pct * max_kbd_backlight) == 0) {
            kbd_msg.curr = state.current_kbd_pct;
            emit_prop(current_kbd_topic, self(), &kbd_msg, sizeof(bl_upd));
        }
    }
}

void set_backlight_level(const double pct, const int is_smooth, const double step, const int timeout) {
    SYSBUS_ARG(args, CLIGHTD_SERVICE, "/org/clightd/clightd/Backlight", "org.clightd.clightd.Backlight", "SetAll");

    /* Set backlight on both internal monitor (in case of laptop) and external ones */
    int ok;
    int r = call(&ok, "b", &args, "d(bdu)s", pct, is_smooth, step, timeout, conf.screen_path);
    if (!r && ok) {
        state.current_bl_pct = pct;
        bl_msg.curr = pct;
        emit_prop(current_bl_topic, self(), &bl_msg, sizeof(bl_upd));
    }
}

static int capture_frames_brightness(void) {
    SYSBUS_ARG(args, CLIGHTD_SERVICE, "/org/clightd/clightd/Sensor", "org.clightd.clightd.Sensor", "Capture");
    double intensity[conf.num_captures];
    int r = call(intensity, "sad", &args, "si", conf.dev_name, conf.num_captures);
    if (!r) {
        state.ambient_br = compute_average(intensity, conf.num_captures);
        amb_msg.curr = state.ambient_br;
        emit_prop(current_ab_topic, self(), &amb_msg, sizeof(bl_upd));
    }
    return r;
}

/* Callback on upower ac state changed signal */
static void upower_callback(void) {
    set_timeout(0, 1, bl_fd, 0);
}

/* Callback on "Calibrate" bus interface method */
static void interface_calibrate_callback(void) {
    if (!state.display_state && is_sensor_available()) {
        do_capture(0);
    }
}

/* Callback on "AutoCalib" bus exposed writable property */
static void interface_autocalib_callback(void) {
    if (conf.no_auto_calib) {
        pause_mod();
    } else {
        resume_mod();
    }
}

/* Callback on "AcCurvePoints" and "BattCurvePoints" bus exposed writable properties */
static void interface_curve_callback(enum ac_states s) {
    polynomialfit(s);
}

/* Callback on "backlight_timeout" bus exposed writable properties */
static void interface_timeout_callback(int old_val) {
    reset_timer(bl_fd, old_val, curr_timeout);
}

/* Callback on state.display_state changes */
static void dimmed_callback(void) {
    if (state.display_state) {
        pause_mod();
    } else {
        resume_mod();
    }
}

/* Callback on state.time/state.in_event changes */
static void time_callback(void) {
    /* A state.time change happened, react! */
    reset_timer(bl_fd, curr_timeout, get_current_timeout());
    curr_timeout = get_current_timeout();
}

/* Callback on SensorChanged clightd signal */
static int on_sensor_change(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int new_sensor_avail = is_sensor_available();
    if (new_sensor_avail != sensor_available) {
        sensor_available = new_sensor_avail;
        if (sensor_available) {
            INFO("Resumed as a sensor is available.\n");
            resume_mod();
        } else {
            INFO("Paused as no sensor is available.\n");
            pause_mod();
        }
    }
    return 0;
}

static inline int get_current_timeout(void) {
    if (state.in_event) {
        return conf.timeout[state.ac_state][IN_EVENT];
    }
    return conf.timeout[state.ac_state][state.time];
}

static void pause_mod(void) {
    m_tell(self(), &pause_msg, sizeof(state_upd), false);
    m_become(paused);
}

static void resume_mod(void) {
    m_unbecome();
    m_tell(self(), &resume_msg, sizeof(state_upd), false);
}
