#include "interface.h"
#include "my_math.h"
#include "utils.h"

static void receive_waiting_init(const msg_t *const msg, UNUSED const void* userdata);
static void receive_paused(const msg_t *const msg, const void* userdata);
static void init_curves(void);
static int parse_bus_reply(sd_bus_message *reply, const char *member, void *userdata);
static int is_sensor_available(void);
static void do_capture(bool reset_timer, bool capture_only);
static void set_new_backlight(void);
static void publish_bl_upd(const double pct, const bool is_smooth, const double step, const int timeout);
static void set_each_brightness(double pct, const double step, const int timeout);
static void set_backlight_level(const double pct, const bool is_smooth, double step, int timeout);
static int capture_frames_brightness(void);
static void upower_callback(void);
static void interface_autocalib_callback(bool new_val);
static void reset_or_pause(int old_timeout, bool reset);
static void interface_curve_callback(double *regr_points, int num_points, enum ac_states s);
static void interface_timeout_callback(timeout_upd *up);
static void dimmed_callback(void);
static void suspended_callback(void);
static void inhibit_callback(void);
static void time_callback(int old_val, const bool is_event);
static int on_sensor_change(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int on_bl_changed(sd_bus_message *m, UNUSED void *userdata, UNUSED sd_bus_error *ret_error);
static int on_interface_added(sd_bus_message *m, UNUSED void *userdata, UNUSED sd_bus_error *ret_error);
static int on_interface_removed(sd_bus_message *m, UNUSED void *userdata, UNUSED sd_bus_error *ret_error);
static void on_delayed_interface(void);
static int get_current_timeout(void);
static void on_lid_update(void);
static void pause_mod(enum mod_pause type);
static void resume_mod(enum mod_pause type);
static int set_auto_calib(sd_bus *bus, const char *path, const char *interface, const char *property,
                          sd_bus_message *value, void *userdata, sd_bus_error *error);
static int method_list_mon_override(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_set_mon_override(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

static map_t *bls;
static int bl_fd = -1, delayed_fd;
static sd_bus_slot *sens_slot, *bl_slot, *if_a_slot, *if_r_slot;
static char *backlight_interface; // main backlight interface used to only publish BL_UPD msgs for a single backlight sn
static const sd_bus_vtable conf_bl_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_WRITABLE_PROPERTY("NoAutoCalib", "b", NULL, set_auto_calib, offsetof(bl_conf_t, no_auto_calib), 0),
    SD_BUS_WRITABLE_PROPERTY("InhibitOnLidClosed", "b", NULL, NULL, offsetof(bl_conf_t, pause_on_lid_closed), 0),
    SD_BUS_WRITABLE_PROPERTY("CaptureOnLidOpened", "b", NULL, NULL, offsetof(bl_conf_t, capture_on_lid_opened), 0),
    SD_BUS_WRITABLE_PROPERTY("NoSmooth", "b", NULL, NULL, offsetof(bl_conf_t, smooth.no_smooth), 0),
    SD_BUS_WRITABLE_PROPERTY("TransStep", "d", NULL, NULL, offsetof(bl_conf_t, smooth.trans_step), 0),
    SD_BUS_WRITABLE_PROPERTY("TransDuration", "i", NULL, NULL, offsetof(bl_conf_t, smooth.trans_timeout), 0),
    SD_BUS_WRITABLE_PROPERTY("TransFixed", "i", NULL, NULL, offsetof(bl_conf_t, smooth.trans_fixed), 0),
    SD_BUS_WRITABLE_PROPERTY("ShutterThreshold", "d", NULL, NULL, offsetof(bl_conf_t, shutter_threshold), 0),
    SD_BUS_WRITABLE_PROPERTY("AcDayTimeout", "i", NULL, set_timeouts, offsetof(bl_conf_t, timeout[ON_AC][DAY]), 0),
    SD_BUS_WRITABLE_PROPERTY("AcNightTimeout", "i", NULL, set_timeouts, offsetof(bl_conf_t, timeout[ON_AC][NIGHT]), 0),
    SD_BUS_WRITABLE_PROPERTY("AcEventTimeout", "i", NULL, set_timeouts, offsetof(bl_conf_t, timeout[ON_AC][IN_EVENT]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattDayTimeout", "i", NULL, set_timeouts, offsetof(bl_conf_t, timeout[ON_BATTERY][DAY]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattNightTimeout", "i", NULL, set_timeouts, offsetof(bl_conf_t, timeout[ON_BATTERY][NIGHT]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattEventTimeout", "i", NULL, set_timeouts, offsetof(bl_conf_t, timeout[ON_BATTERY][IN_EVENT]), 0),
    SD_BUS_WRITABLE_PROPERTY("RestoreOnExit", "b", NULL, NULL, offsetof(bl_conf_t, restore), 0),
    SD_BUS_VTABLE_END
};

static const sd_bus_vtable conf_sens_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_WRITABLE_PROPERTY("Device", "s", NULL, NULL, offsetof(sensor_conf_t, dev_name), 0),
    SD_BUS_WRITABLE_PROPERTY("Settings", "s", NULL, NULL, offsetof(sensor_conf_t, dev_opts), 0),
    SD_BUS_WRITABLE_PROPERTY("AcCaptures", "i", NULL, NULL, offsetof(sensor_conf_t, num_captures[ON_AC]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattCaptures", "i", NULL, NULL, offsetof(sensor_conf_t, num_captures[ON_BATTERY]), 0),
    SD_BUS_WRITABLE_PROPERTY("AcPoints", "ad", get_curve, set_curve, offsetof(sensor_conf_t, default_curve[ON_AC]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattPoints", "ad", get_curve, set_curve, offsetof(sensor_conf_t, default_curve[ON_BATTERY]), 0),
    SD_BUS_VTABLE_END
};

static const sd_bus_vtable conf_mon_override_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("List", NULL, "a(sadad)", method_list_mon_override, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Set", "sadad", NULL, method_set_mon_override, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

DECLARE_MSG(amb_msg, AMBIENT_BR_UPD);
DECLARE_MSG(capture_req, CAPTURE_REQ);
DECLARE_MSG(sens_msg, SENS_UPD);
DECLARE_MSG(calib_req, NO_AUTOCALIB_REQ);
DECLARE_MSG(bl_req, BL_REQ);
DECLARE_MSG(no_autocalib_upd, NO_AUTOCALIB_UPD);

API(Backlight, conf_bl_vtable, conf.bl_conf);
API(Sensor, conf_sens_vtable, conf.sens_conf);
API(MonitorOverride, conf_mon_override_vtable, conf.sens_conf.specific_curves);
MODULE_WITH_PAUSE("BACKLIGHT");

static void init(void) {
    bls = map_new(true, free);
    capture_req.capture.reset_timer = true;
    bl_req.bl.smooth = -1; // Use conf values
    
    delayed_fd = start_timer(CLOCK_BOOTTIME, 0, 0);
    
    // Disabled while in wizard mode as it is useless and spams to stdout
    if (!conf.wizard) {
        init_curves();
        init_Backlight_api();
        init_Sensor_api();
        init_MonitorOverride_api();
    }

    M_SUB(UPOWER_UPD);
    M_SUB(DISPLAY_UPD);
    M_SUB(LID_UPD);
    M_SUB(SUSPEND_UPD);
    M_SUB(DAYTIME_UPD);
    M_SUB(IN_EVENT_UPD);
    M_SUB(BL_TO_REQ);
    M_SUB(CAPTURE_REQ);
    M_SUB(CURVE_REQ);
    M_SUB(NO_AUTOCALIB_REQ);
    M_SUB(BL_REQ);
    M_SUB(INHIBIT_UPD);
    M_SUB(SCREEN_BR_UPD);
    m_become(waiting_init);
}

static bool check(void) {
    return true;
}

static bool evaluate(void) {
    return !conf.bl_conf.disabled;
}

static void destroy(void) {
    if (sens_slot) {
        sens_slot = sd_bus_slot_unref(sens_slot);
    }
    if (bl_slot) {
        bl_slot = sd_bus_slot_unref(bl_slot);
    }
    if (if_a_slot) {
        if_a_slot = sd_bus_slot_unref(if_a_slot);
    }
    if (if_r_slot) {
        if_r_slot = sd_bus_slot_unref(if_r_slot);
    }
    deinit_Backlight_api();
    deinit_Sensor_api();
    deinit_MonitorOverride_api();
    if (bl_fd >= 0) {
        close(bl_fd);
    }
    close(delayed_fd);
    free(backlight_interface);
    free(conf.sens_conf.dev_name);
    free(conf.sens_conf.dev_opts);
    map_free(conf.sens_conf.specific_curves);
}

static void receive_waiting_init(const msg_t *const msg, UNUSED const void* userdata) {
    static enum { UPOWER_STARTED = 1 << 0, LID_STARTED = 1 << 1, DAYTIME_STARTED = 1 << 2, SCREEN_STARTED = 1 << 3, ALL_STARTED = (1 << 4) - 1} ok = 0;
    static const self_t *wizSelf = NULL, *screenSelf = NULL;
    if (!wizSelf) {
        m_ref("WIZARD", &wizSelf);
    }
    if (!screenSelf) {
        m_ref("SCREEN", &screenSelf);
    }
    
    if (conf.screen_conf.disabled) {
        ok |= SCREEN_STARTED;
    }

    switch (MSG_TYPE()) {
    case UPOWER_UPD:
        ok |= UPOWER_STARTED;
        break;
    case LID_UPD:
        ok |= LID_STARTED;
        break;
    case DAYTIME_UPD:
        ok |= DAYTIME_STARTED;
        break;
    case SCREEN_BR_UPD:
        ok |= SCREEN_STARTED;
        break;
    case SYSTEM_UPD:
        /* 
         * When wizard gets started, it means we are in wizard mode.
         * In wizard mode, we won't receive UPOWER_UPD, LID_UPD or DAYTIME_UPD
         * messages. Go on anyway.
         */
        if (msg->ps_msg->type == MODULE_STARTED &&
            msg->ps_msg->sender == wizSelf) {
            
            ok = ALL_STARTED;
        } else if (msg->ps_msg->type == MODULE_STOPPED && 
            msg->ps_msg->sender == screenSelf) {
           // Screen was deregistered (unsupported).
            ok |= SCREEN_STARTED; 
        }
        break;
    default:
        break;
    }
    
    /* Wait on each of these 3 messages before actually starting up */
    if (ok == ALL_STARTED) {
        m_unbecome();
        
        /* We do not fail if this fails */
        SYSBUS_ARG(sens_args, CLIGHTD_SERVICE, "/org/clightd/clightd/Sensor", "org.clightd.clightd.Sensor", "Changed");
        add_match(&sens_args, &sens_slot, on_sensor_change);
        
        SYSBUS_ARG(bl_args, CLIGHTD_SERVICE, "/org/clightd/clightd/Backlight2", "org.clightd.clightd.Backlight2", "Changed");
        add_match(&bl_args, &bl_slot, on_bl_changed);
        
        SYSBUS_ARG(if_added_args, CLIGHTD_SERVICE, "/org/clightd/clightd/Backlight2", "org.freedesktop.DBus.ObjectManager", "InterfacesAdded");
        add_match(&if_added_args, &if_a_slot, on_interface_added);
        
        SYSBUS_ARG(if_removed_args, CLIGHTD_SERVICE, "/org/clightd/clightd/Backlight2", "org.freedesktop.DBus.ObjectManager", "InterfacesRemoved");
        add_match(&if_removed_args, &if_r_slot, on_interface_removed);
        
        /* Create the timerfd and eventually pause (if current timeout is <0) */
        bl_fd = start_timer(CLOCK_BOOTTIME, 0, get_current_timeout() > 0);
        m_register_fd(bl_fd, false, NULL);
        reset_or_pause(-1, false);
        
        /* Eventually pause backlight if sensor is not available */
        on_sensor_change(NULL, NULL, NULL);
        
        if (conf.bl_conf.no_auto_calib) {
            /*
             * If automatic calibration is disabled, we need to ensure to start
             * from a well known backlight level for DIMMER to correctly work.
             * Force 100% backlight level.
             *
             * Note: set smooth field from conf to allow keyboard and gamma to react;
             * > if (up->smooth || conf.bl_conf.no_smooth) { ... }
             */
            set_backlight_level(1.0, !conf.bl_conf.smooth.no_smooth, 0, 0);
            pause_mod(AUTOCALIB);
        }
        if (state.lid_state) {
            /* 
             * If we start with closed lid,
             * pause backlight calibration if configured.
             */
            on_lid_update();
        }
        
        // Store current backlight to later restore them if requested
        SYSBUS_ARG_REPLY(args, parse_bus_reply, NULL, CLIGHTD_SERVICE, "/org/clightd/clightd/Backlight2", "org.clightd.clightd.Backlight2", "Get");
        call(&args, NULL);
    }
}

static void receive(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
    case FD_UPD:
        read_timer(msg->fd_msg->fd);
        if (msg->fd_msg->fd == delayed_fd) {
            on_delayed_interface();
        } else {
            // When SCREEN module is running, capture only!
            capture_req.capture.capture_only = state.screen_br != 0.0f;
            M_PUB(&capture_req);
        }
        break;
    case SCREEN_BR_UPD:
        set_new_backlight();
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
    case LID_UPD:
        on_lid_update();
        break;
    case SUSPEND_UPD:
        suspended_callback();
        break;
    case INHIBIT_UPD:
        inhibit_callback();
        break;
    case BL_TO_REQ: {
        timeout_upd *up = (timeout_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            interface_timeout_callback(up);
        }
        break;
    }
    case CAPTURE_REQ: {
        capture_upd *up = (capture_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            do_capture(up->reset_timer, up->capture_only);
        }
        break;
    }
    case CURVE_REQ: {
        curve_upd *up = (curve_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            interface_curve_callback(up->regression_points, up->num_points, up->state);
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
    case SYSTEM_UPD:
        if (msg->ps_msg->type == LOOP_STOPPED && conf.bl_conf.restore) {
            set_each_brightness(-1.0f, 0, 0);
        }
        break; 
    default:
        break;
    }
}

static void receive_paused(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
    case FD_UPD:
        // While paused, we can only receive events from delayed_fd!
        read_timer(msg->fd_msg->fd);
        on_delayed_interface();
        break;
    case SCREEN_BR_UPD:
        if (!state.display_state) {
            set_new_backlight();
        }
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
    case LID_UPD:
        on_lid_update();
        break;
    case SUSPEND_UPD:
        suspended_callback();
        break;
    case INHIBIT_UPD:
        inhibit_callback();
        break;
    case BL_TO_REQ: {
        timeout_upd *up = (timeout_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            interface_timeout_callback(up);
        }
        break;
    }
    case CAPTURE_REQ: {
        capture_upd *up = (capture_upd *)MSG_DATA();
        /* In paused state check that we're not dimmed/dpms and sensor is available */
        if (VALIDATE_REQ(up) && !state.display_state && state.sens_avail) {
            do_capture(up->reset_timer, up->capture_only);
        }
        break;
    }
    case CURVE_REQ: {
        curve_upd *up = (curve_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            interface_curve_callback(up->regression_points, up->num_points, up->state);
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
    case SYSTEM_UPD:
        if (msg->ps_msg->type == LOOP_STOPPED && conf.bl_conf.restore) {
            set_each_brightness(-1.0f, 0, 0);
        }
        break; 
    default:
        break;
    }
}

static void init_curves(void) {
    /* Compute polynomial best-fit parameters */
    interface_curve_callback(NULL, 0, ON_AC);
    interface_curve_callback(NULL, 0, ON_BATTERY);
    
    char tag[128] = {0};
    /* Compute polynomial best-fit parameters for specific monitor backlight adjustments */
    for (map_itr_t *itr = map_itr_new(conf.sens_conf.specific_curves); itr; itr = map_itr_next(itr)) {
        const char *sn = map_itr_get_key(itr);
        curve_t *c = map_itr_get_data(itr);
        
        snprintf(tag, sizeof(tag), "AC '%s' backlight", sn);
        polynomialfit(NULL, &c[ON_AC], tag);
        snprintf(tag, sizeof(tag), "BATT '%s' backlight", sn);
        polynomialfit(NULL, &c[ON_BATTERY], tag);
    } 
}

static int parse_bus_reply(sd_bus_message *reply, const char *member, void *userdata) {
    int r = -EINVAL;
    if (!strcmp(member, "IsAvailable")) {
        const char *sensor = NULL;
        r = sd_bus_message_read(reply, "sb", &sensor, userdata);
        int is_avail = *((int *)userdata);
        if (r >= 0 && is_avail) {
            DEBUG("Sensor '%s' is now available.\n", sensor);
        }
    } else if (!strcmp(member, "Capture")) {
        const char *sensor = NULL;
        const double *intensity = NULL;
        size_t length = 0;
        r = sd_bus_message_read(reply, "s", &sensor);
        r += sd_bus_message_read_array(reply, 'd', (const void **)&intensity, &length);
        if (r >= 0) {
            const int num_captures = length / sizeof(double);
            amb_msg.bl.old = state.ambient_br;
            state.ambient_br = compute_average(intensity, num_captures);
            DEBUG("Captured [%d/%d] from '%s'. Ambient brightness: %.3lf.\n", num_captures, 
                  conf.sens_conf.num_captures[state.ac_state], 
                  sensor, state.ambient_br);
            amb_msg.bl.new = state.ambient_br;
            M_PUB(&amb_msg);
        }
    } else if (!strcmp(member, "Get")) {
        r = sd_bus_message_enter_container(reply, SD_BUS_TYPE_ARRAY, "(sd)");
        if (r == 1) {
            while ((r = sd_bus_message_enter_container(reply, SD_BUS_TYPE_STRUCT, "sd") == 1)) {
                const char *mon_id = NULL;
                double *pct = malloc(sizeof(double)),
                r = sd_bus_message_read(reply, "sd", &mon_id, pct);
                if (r >= 0) {
                    char key[PATH_MAX + 1];
                    snprintf(key, sizeof(key), "/org/clightd/clightd/Backlight2/%s", mon_id);
                    map_put(bls, key, pct);
                }
                sd_bus_message_exit_container(reply);
            }
            sd_bus_message_exit_container(reply);
        }
    }
    return r;
}

static int is_sensor_available(void) {
    int available = 0;
    SYSBUS_ARG_REPLY(args, parse_bus_reply, &available, CLIGHTD_SERVICE, "/org/clightd/clightd/Sensor", "org.clightd.clightd.Sensor", "IsAvailable");
    int r = call(&args, "s", conf.sens_conf.dev_name);
    return r == 0 && available;
}

static void do_capture(bool reset_timer, bool capture_only) {
    if (capture_frames_brightness() == 0) {
        if (state.ambient_br >= conf.bl_conf.shutter_threshold) {
            if (!capture_only) {
                set_new_backlight();
            }
        } else {
            INFO("Ambient brightness: %.3lf -> Clogged capture detected.\n", state.ambient_br);
        }
    }

    if (reset_timer) {
        set_timeout(get_current_timeout(), 0, bl_fd, 0);
    }
}

static void set_new_backlight(void) {
    curve_t *curve = &conf.sens_conf.default_curve[state.ac_state];
    
    const double new_bl = get_value_from_curve(state.ambient_br, curve);
    if (state.screen_br == 0.0f) {
        bl_req.bl.new = new_bl;
    } else {
        const double max = curve->points[curve->num_points - 1] > curve->points[0] ? curve->points[curve->num_points - 1] : curve->points[0];
        const double min = curve->points[0] < curve->points[curve->num_points - 1] ? curve->points[0] : curve->points[curve->num_points - 1];
        const double wmax = new_bl + conf.screen_conf.contrib;
        bl_req.bl.new = clamp(wmax - (2 * conf.screen_conf.contrib * state.screen_br), max, min);
        DEBUG("Content calib: wmax: %.3lf, wmin: %.3lf, new_bl: %.3lf\n", 
              wmax, wmax - 2 * conf.screen_conf.contrib, bl_req.bl.new);
    }
    // Less verbose: only log real backlight changes, unless we are in verbose mode
    if (bl_req.bl.new != state.current_bl_pct || conf.verbose) {
        if (state.screen_br == 0.0f) {
            INFO("Ambient brightness: %.3lf -> Screen backlight: %.3lf.\n", state.ambient_br, bl_req.bl.new);
        } else {
            INFO("Ambient brightness: %.3lf, Screen brightness: %.3lf -> Screen backlight: %.3lf.\n", 
                 state.ambient_br, state.screen_br, bl_req.bl.new);
        }
        M_PUB(&bl_req);
    }
}

static void publish_bl_upd(const double pct, const bool is_smooth, const double step, const int timeout) {
    DECLARE_HEAP_MSG(bl_msg, BL_UPD);
    bl_msg->bl.old = state.current_bl_pct;
    bl_msg->bl.new = pct;
    bl_msg->bl.smooth = is_smooth;
    bl_msg->bl.step = step;
    bl_msg->bl.timeout = timeout;
    M_PUB(bl_msg);
}

static void set_each_brightness(double pct, const double step, const int timeout) {
    const bool restoring = pct == -1.0f;
    sensor_conf_t *sens_conf = &conf.sens_conf;
    enum ac_states st = state.ac_state;
    
    for (map_itr_t *itr = map_itr_new(bls); itr; itr = map_itr_next(itr)) {
        const char *path = map_itr_get_key(itr);
        double *val = map_itr_get_data(itr);
        
        const char *mon_id = strrchr(path, '/') + 1;
        
        /* Set backlight on monitor id */
        SYSBUS_ARG(args, CLIGHTD_SERVICE, path, "org.clightd.clightd.Backlight2.Server", "Set");
        curve_t *c = map_get(sens_conf->specific_curves, mon_id);
        
        int r;
        /*
         * Only if a specific curve has been found and 
         * we are not restoring a previously saved backlight level 
         */
        if (c && !restoring) {
            /* Use monitor specific adjustment, properly scaling bl pct */
            const double real_pct = get_value_from_curve(pct, &c[st]);
            DEBUG("Using specific curve for '%s': setting %.3lf pct.\n", mon_id, real_pct);
            r = call(&args, "d(du)", real_pct, step, timeout);
        } else {
            DEBUG("Using default curve for '%s'\n", mon_id);
            /* Use non-adjusted (default) curve value */
            r = call(&args, "d(du)", restoring ? *val : pct, step, timeout);
        }
        if (r < 0) {
            WARN("Failed to set backlight on %s.\n", mon_id);
        }
    }
}

static void set_backlight_level(const double pct, const bool is_smooth, double step, int timeout) {
    int r = -EINVAL;
    if (!is_smooth) {
        step = 0;
        timeout = 0;
    }
    if (map_length(conf.sens_conf.specific_curves) > 0) {
        set_each_brightness(pct, step, timeout);
    } else {
        SYSBUS_ARG(args, CLIGHTD_SERVICE, "/org/clightd/clightd/Backlight2", "org.clightd.clightd.Backlight2", "Set");
        
        /* Set backlight on both internal monitor (in case of laptop) and external ones */
        r = call(&args, "d(du)", pct, step, timeout);
    }
    
    if (r >= 0 && is_smooth) {
        // Publish smooth target and params
        publish_bl_upd(pct, true, step, timeout);
    }
}

static int capture_frames_brightness(void) {
    SYSBUS_ARG_REPLY(args, parse_bus_reply, NULL, CLIGHTD_SERVICE, "/org/clightd/clightd/Sensor", "org.clightd.clightd.Sensor", "Capture");
    return call(&args, "sis", conf.sens_conf.dev_name, 
                conf.sens_conf.num_captures[state.ac_state], 
                conf.sens_conf.dev_opts);
}

/* Callback on upower ac state changed signal */
static void upower_callback(void) {
    set_timeout(0, get_current_timeout() > 0, bl_fd, 0);
}

/* Callback on "NoAutoCalib" bus exposed writable property */
static void interface_autocalib_callback(bool new_val) {
    INFO("Backlight autocalibration %s.\n", new_val ? "disabled" : "enabled");
    conf.bl_conf.no_auto_calib = new_val;
    if (conf.bl_conf.no_auto_calib) {
        pause_mod(AUTOCALIB);
    } else {
        resume_mod(AUTOCALIB);
    }
    no_autocalib_upd.nocalib.new = new_val;
    M_PUB(&no_autocalib_upd);
}

/* Callback on "AcCurvePoints" and "BattCurvePoints" bus exposed writable properties */
static void interface_curve_callback(double *regr_points, int num_points, enum ac_states s) {
    // Only default curve can be changed through api 
    curve_t *c = &conf.sens_conf.default_curve[s];
    if (regr_points) {
        memcpy(c->points, 
           regr_points, num_points * sizeof(double));
        c->num_points = num_points;
    }
    polynomialfit(NULL, c, s == ON_AC ? "AC screen backlight" : "BATT screen backlight");
}

/* 
 * This internal API allows
 * to pause BACKLIGHT if <= 0 timeout should be set,
 * else resuming it and setting correct timeout.
 */
static void reset_or_pause(int old_timeout, bool reset) {
    const int new_timeout = get_current_timeout();
    if (new_timeout <= 0) {
        pause_mod(TIMEOUT);
    } else {
        resume_mod(TIMEOUT);
        if (reset) {
            reset_timer(bl_fd, old_timeout, new_timeout);
        }
    }
}

/* Callback on "backlight_timeout" bus exposed writable properties */
static void interface_timeout_callback(timeout_upd *up) {
    /* Validate request: BACKLIGHT is the only module that requires valued daytime */
    if (up->daytime >= DAY && up->daytime <= SIZE_STATES) {
        const int old = get_current_timeout();
        conf.bl_conf.timeout[up->state][up->daytime] = up->new;
        // Check if current timeout was updated
        if (up->state == state.ac_state && 
            (up->daytime == state.day_time || (state.in_event && up->daytime == IN_EVENT))) {
            
            reset_or_pause(old, true);
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

static void suspended_callback(void) {
    if (state.suspended) {
        pause_mod(SUSPEND);
    } else {
        resume_mod(SUSPEND);
    }
}

static void inhibit_callback(void) {
    if (state.inhibited && conf.inh_conf.inhibit_bl) {
        pause_mod(INHIBIT);
    } else {
        resume_mod(INHIBIT);
    }
}

/* Callback on state.time/state.in_event changes */
static void time_callback(int old_val, const bool is_event) {
    int old_timeout;
    if (!is_event) {
        /* A state.time change happened, react! */
        old_timeout = conf.bl_conf.timeout[state.ac_state][old_val];
    } else {
        /* A state.in_event change happened, react!
         * If state.in_event is now true, it means we were in state.time timeout.
         * Else, an event ended, thus we were IN_EVENT.
         */
        old_timeout = conf.bl_conf.timeout[state.ac_state][state.in_event ? state.day_time : IN_EVENT];
    }
     reset_or_pause(old_timeout, true);
}

/* Callback on SensorChanged clightd signal */
static int on_sensor_change(UNUSED sd_bus_message *m, UNUSED void *userdata, UNUSED sd_bus_error *ret_error) {
    int new_sensor_avail = is_sensor_available();
    if (new_sensor_avail != state.sens_avail) {
        sens_msg.sens.old = state.sens_avail;
        sens_msg.sens.new = new_sensor_avail;
        M_PUB(&sens_msg);
        state.sens_avail = new_sensor_avail;
        if (state.sens_avail) {
            DEBUG("Resumed as a sensor is now available.\n");
            resume_mod(SENSOR);
        } else {
            DEBUG("Paused as no sensor is available.\n");
            pause_mod(SENSOR);
        }
    }
    return 0;
}

static int on_bl_changed(sd_bus_message *m, UNUSED void *userdata, UNUSED sd_bus_error *ret_error) {
    double pct;
    const char *syspath = NULL;
    sd_bus_message_read(m, "sd", &syspath, &pct);

    if (!backlight_interface) {
        backlight_interface = strdup(syspath);
    }
    
    DEBUG("Backlight '%s' level updated: %.2lf.\n", syspath, pct);
    
    /* Publish a single bl update event on multimonitor setups */
    if (!strcmp(backlight_interface, syspath)) {
        publish_bl_upd(pct, false, 0, 0);
        state.current_bl_pct = pct;
    }
    return 0;
}

static int on_interface_added(sd_bus_message *m, UNUSED void *userdata, UNUSED sd_bus_error *ret_error) {
    const char *obj_path;
    if (sd_bus_message_read(m, "o", &obj_path) >= 0) {
        DEBUG("Backlight %s added.\n", obj_path);
        double *val = malloc(sizeof(double));
        /* 
         * Initial default value for newly attached screen is 1.0; 
         * in case of restore, 100% backlight level will be set.
         */
        *val = 1.0;
        map_put(bls, obj_path, val);
        
        if (conf.bl_conf.sync_monitors_delay > 0) {
            set_timeout(conf.bl_conf.sync_monitors_delay, 0, delayed_fd, 0);
            m_register_fd(delayed_fd, false, NULL);
        } else {
            on_delayed_interface();
        }
    }
    return 0;
}

static int on_interface_removed(sd_bus_message *m, UNUSED void *userdata, UNUSED sd_bus_error *ret_error) {
    const char *obj_path;
    if (sd_bus_message_read(m, "o", &obj_path) >= 0) {
        DEBUG("Backlight %s removed.\n", obj_path);
        
        // Check if backlight_interface was currently using the removed object; in case, remove it.
        if (strcmp(strrchr(obj_path, '/') + 1, backlight_interface) == 0) {
            free(backlight_interface);
            backlight_interface = NULL;
        }
        map_remove(bls, obj_path);
        
        if (conf.bl_conf.sync_monitors_delay > 0) {
            set_timeout(conf.bl_conf.sync_monitors_delay, 0, delayed_fd, 0);
            m_register_fd(delayed_fd, false, NULL);
        } else {
            on_delayed_interface();
        }
    }
    return 0;
}

static void on_delayed_interface(void) {
    /* Force-set gamma temp on all monitors */
    DECLARE_HEAP_MSG(temp_req, TEMP_REQ);
    temp_req->temp.new = state.current_temp;
    temp_req->temp.smooth = -1;
    temp_req->temp.daytime = -1;
    M_PUB(temp_req);
    
    /* Force-set backlight on all monitors */
    DECLARE_HEAP_MSG(bl_req, BL_REQ);
    bl_req->bl.new = state.current_bl_pct;
    bl_req->bl.smooth = -1;
    M_PUB(bl_req);
    
    m_deregister_fd(delayed_fd);
}

static inline int get_current_timeout(void) {
    if (state.in_event) {
        return conf.bl_conf.timeout[state.ac_state][IN_EVENT];
    }
    return conf.bl_conf.timeout[state.ac_state][state.day_time];
}

static void on_lid_update(void) {
    if (state.lid_state) {
        if (conf.bl_conf.pause_on_lid_closed) {
            pause_mod(LID);
        }
    } else {
        if (conf.bl_conf.capture_on_lid_opened) {
            /* Fire immediately */
            set_timeout(0, 1, bl_fd, 0);
        }
        resume_mod(LID);
    }
}

static void pause_mod(enum mod_pause type) {
    if (CHECK_PAUSE(true, type)) {
        m_become(paused);
        /* Properly deregister our fd while paused */
        m_deregister_fd(bl_fd);
    }
}

static void resume_mod(enum mod_pause type) {
    if (CHECK_PAUSE(false, type)) {
        m_unbecome();
        /* Register back our fd on resume */
        m_register_fd(bl_fd, false, NULL);
    }
}

static int set_auto_calib(sd_bus *bus, const char *path, const char *interface, const char *property,
                          sd_bus_message *value, void *userdata, sd_bus_error *error) {
    VALIDATE_PARAMS(value, "b", &calib_req.nocalib.new);
    
    M_PUB(&calib_req);
    return r;
}

static int method_list_mon_override(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    map_t *curves = (map_t *)userdata;
    
    sd_bus_message *reply = NULL;
    sd_bus_message_new_method_return(m, &reply);
    sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "(sadad)");
    for (map_itr_t *itr = map_itr_new(curves); itr; itr = map_itr_next(itr)) {
        sd_bus_message_open_container(reply, SD_BUS_TYPE_STRUCT, "sadad");
        const char *sn = map_itr_get_key(itr);
        sd_bus_message_append(reply, "s", sn);
        for (int st = ON_AC; st < SIZE_AC; st++) {
            sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "d");
            curve_t *c = map_itr_get_data(itr);
            for (int i = 0; i < c[st].num_points; i++) {
                sd_bus_message_append(reply, "d", c[st].points[i]);
            }
            sd_bus_message_close_container(reply);
        }
        sd_bus_message_close_container(reply);
    }
    sd_bus_message_close_container(reply);
    sd_bus_send(NULL, reply, NULL);
    sd_bus_message_unref(reply);
    return 0;
}

static int method_set_mon_override(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    map_t *curves = (map_t *)userdata;
    
    const char *sn = NULL;
    VALIDATE_PARAMS(m, "s", &sn);
    
    double *data[SIZE_AC] = { NULL, NULL };
    size_t num_points[SIZE_AC] = {0};
    for (int st = ON_AC; st < SIZE_AC; st++) {
        size_t length;
        r = sd_bus_message_read_array(m, 'd', (const void**) &data[st], &length);
        if (r < 0) {
            WARN("Failed to parse parameters: %s\n", strerror(-r));
            return r;
        }
        
        num_points[st] = length / sizeof(double);
        
        if (num_points[st] > MAX_SIZE_POINTS) {
            sd_bus_error_set_errno(ret_error, EINVAL);
            return -EINVAL;
        }
        
        // validate curve data
        for (int i = 0; i < num_points[st]; i++) {
            if (data[st][i] < 0.0 || data[st][i] > 1.0) {
                sd_bus_error_set_errno(ret_error, EINVAL);
                return -EINVAL;
            }
        }
    }
    
    // they must be both != 0 or 0
    if (num_points[ON_AC] ^ num_points[ON_BATTERY]) {
        sd_bus_error_set_errno(ret_error, EINVAL);
        return -EINVAL;
    }
    
    // remove curve if existent
    if (num_points[ON_AC] == 0) {
        if (!map_has_key(curves, sn)) {
            sd_bus_error_set_errno(ret_error, ENOENT);
            return -ENOENT;
        }
        map_remove(curves, sn);
        return sd_bus_reply_method_return(m, NULL);
    }
    
    curve_t *c = map_get(curves, sn);
    if (!c) {
        c = calloc(SIZE_AC, sizeof(curve_t));
        if (!c) {
            sd_bus_error_set_errno(ret_error, ENOMEM);
            return -ENOMEM;
        }
    }
    
    char tag[128] = {0};
    for (int st = ON_AC; st < SIZE_AC; st++) {
        c[st].num_points = num_points[st];
        memcpy(c[st].points, data[st], num_points[st] * sizeof(double));
        snprintf(tag, sizeof(tag), "%s '%s' backlight", st == ON_AC ? "AC" : "BATT", sn);
        polynomialfit(NULL, &c[st], tag);
    }
    
    map_put(curves, sn, c);
    
    return sd_bus_reply_method_return(m, NULL);
}
