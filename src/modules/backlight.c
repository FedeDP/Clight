#include "bus.h"
#include "my_math.h"
#include "utils.h"

static void receive_waiting_init(const msg_t *const msg, UNUSED const void* userdata);
static void receive_paused(const msg_t *const msg, const void* userdata);
static void init_curves(void);
static int parse_bus_reply(sd_bus_message *reply, const char *member, void *userdata);
static int is_sensor_available(void);
static void do_capture(bool reset_timer, bool capture_only);
static double set_new_backlight(const double perc);
static void publish_bl_upd(const double pct, const bool is_smooth, const double step, const int timeout);
static int get_and_set_each_brightness(const double pct, const bool is_smooth, const double step, const int timeout);
static void set_each_brightness(sd_bus_message *m, double pct, const bool is_smooth, const double step, const int timeout);
static void set_backlight_level(const double pct, const bool is_smooth, const double step, const int timeout);
static int capture_frames_brightness(void);
static void upower_callback(void);
static void interface_autocalib_callback(bool new_val);
static void reset_or_pause(int old_timeout);
static void interface_curve_callback(double *regr_points, int num_points, enum ac_states s);
static void interface_timeout_callback(timeout_upd *up);
static void dimmed_callback(void);
static void suspended_callback(void);
static void inhibit_callback(void);
static void time_callback(int old_val, const bool is_event);
static int on_sensor_change(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int on_bl_changed(sd_bus_message *m, UNUSED void *userdata, UNUSED sd_bus_error *ret_error);
static int get_current_timeout(void);
static void on_lid_update(void);
static void pause_mod(enum mod_pause type);
static void resume_mod(enum mod_pause type);

static sd_bus_message *restore_m = NULL;
static int bl_fd = -1;
static sd_bus_slot *sens_slot, *bl_slot;
static char *backlight_interface; // main backlight interface used to only publish BL_UPD msgs for a single backlight sn

DECLARE_MSG(amb_msg, AMBIENT_BR_UPD);
DECLARE_MSG(capture_req, CAPTURE_REQ);
DECLARE_MSG(sens_msg, SENS_UPD);

MODULE_WITH_PAUSE("BACKLIGHT");

static void module_pre_start(void) {
    state.sens_avail = -1;
}

static void init(void) {
    capture_req.capture.reset_timer = true;
    capture_req.capture.capture_only = false;
    
    // Disabled while in wizard mode as it is useless and spams to stdout
    if (!conf.wizard) {
        init_curves();
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
    m_become(waiting_init);
    
    // Store current backlight to later restore them if requested
    SYSBUS_ARG_REPLY(args, parse_bus_reply, &restore_m, CLIGHTD_SERVICE, "/org/clightd/clightd/Backlight", "org.clightd.clightd.Backlight", "GetAll");
    call(&args, "s", conf.bl_conf.screen_path);
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
    if (bl_fd >= 0) {
        close(bl_fd);
    }
    if (restore_m) {
        sd_bus_message_unrefp(&restore_m);
    }
    free(backlight_interface);
    free(conf.bl_conf.screen_path);
    free(conf.sens_conf.dev_name);
    free(conf.sens_conf.dev_opts);
    map_free(conf.sens_conf.specific_curves);
}

static void receive_waiting_init(const msg_t *const msg, UNUSED const void* userdata) {
    static enum { UPOWER_STARTED = 1, LID_STARTED = 2, DAYTIME_STARTED = 4, ALL_STARTED = 7} ok = 0;
    static const self_t *wizSelf = NULL;
    if (!wizSelf) {
        m_ref("WIZARD", &wizSelf);
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
    case SYSTEM_UPD:
        /* 
         * When wizard gets started, it means we are in wizard mode.
         * In wizard mode, we won't receive UPOWER_UPD, LID_UPD or DAYTIME_UPD
         * messages. Go on anyway.
         */
        if (msg->ps_msg->type == MODULE_STARTED &&
            msg->ps_msg->sender == wizSelf) {
            
            ok = ALL_STARTED;
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
        
        SYSBUS_ARG(bl_args, CLIGHTD_SERVICE, "/org/clightd/clightd/Backlight", "org.clightd.clightd.Backlight", "Changed");
        add_match(&bl_args, &bl_slot, on_bl_changed);
                
        bl_fd = start_timer(CLOCK_BOOTTIME, 0, get_current_timeout() > 0);
        m_register_fd(bl_fd, false, NULL);
        
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
            set_backlight_level(1.0, !conf.bl_conf.no_smooth, 0, 0);
            pause_mod(AUTOCALIB);
        }
        if (state.lid_state) {
            /* 
             * If we start with closed lid,
             * pause backlight calibration if configured.
             */
            on_lid_update();
        }
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
        if (msg->ps_msg->type == LOOP_STOPPED && restore_m && conf.bl_conf.restore) {
            set_each_brightness(restore_m, 0, false, 0, 0);
            restore_m = NULL;
        }
        break; 
    default:
        break;
    }
}

static void receive_paused(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
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
        if (msg->ps_msg->type == LOOP_STOPPED && restore_m && conf.bl_conf.restore) {
            set_each_brightness(restore_m, 0, false, 0, 0);
            restore_m = NULL;
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
    } else if (!strcmp(member, "SetAll") || !strcmp(member, "Set")) {
        r = sd_bus_message_read(reply, "b", userdata);
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
    } else if (!strcmp(member, "GetAll")) {
        sd_bus_message **msg = (sd_bus_message **)userdata;
        /*
         * Just ref the message to keep it alive 
         * as we will later cycle on returned array.
         */
        sd_bus_message_ref(reply);
        *msg = reply;
        r = 0;
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
    if (!capture_frames_brightness() && !capture_only) {
        /* Account for screen-emitted brightness */
        const double compensated_br = clamp(state.ambient_br - state.screen_comp, 1, 0);
        if (compensated_br >= conf.bl_conf.shutter_threshold) {
            const double new_bl = set_new_backlight(compensated_br);
            if (state.screen_comp > 0.0) {
                INFO("Ambient brightness: %.3lf (-%.3lf screen compensation) -> Backlight pct: %.3lf.\n", state.ambient_br, state.screen_comp, new_bl);
            } else {
                INFO("Ambient brightness: %.3lf -> Backlight pct: %.3lf.\n", state.ambient_br, new_bl);
            }
        } else if (state.screen_comp > 0.0) {
            INFO("Ambient brightness: %.3lf (-%.3lf screen compensation) -> Clogged capture detected.\n", state.ambient_br, state.screen_comp);
        } else {
            INFO("Ambient brightness: %.3lf -> Clogged capture detected.\n", state.ambient_br);
        }
    }

    if (reset_timer) {
        set_timeout(get_current_timeout(), 0, bl_fd, 0);
    }
}

static double set_new_backlight(const double perc) {
    sensor_conf_t *sens_conf = &conf.sens_conf;
    enum ac_states st = state.ac_state;
    const double new_bl_pct = get_value_from_curve(perc, &sens_conf->default_curve[st]);
    set_backlight_level(new_bl_pct, !conf.bl_conf.no_smooth, 
                        conf.bl_conf.trans_step, conf.bl_conf.trans_timeout);
    return new_bl_pct;
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

/* 
 * Calls GetAll on clightd.Backlight to retrieve all currently connected monitors,
 * then for each serial returned tries to find it in specific_curves mapping;
 * if found, maps requested backlight level to desired one for the specific monitor,
 * otherwise just sets requested backlight level (just like a SetAll)
 */
static int get_and_set_each_brightness(const double pct, const bool is_smooth, const double step, const int timeout) {
    /* Get all monitors backlight; we just need to map each monitor to its specific curve indeed */
    // TODO: when clightd will use different object paths for each monitor, just enumerate paths here!
    sd_bus_message *m = NULL;
    SYSBUS_ARG_REPLY(args, parse_bus_reply, &m, CLIGHTD_SERVICE, "/org/clightd/clightd/Backlight", "org.clightd.clightd.Backlight", "GetAll");
    int r = call(&args, "s", conf.bl_conf.screen_path);
    if (r >= 0) {
        set_each_brightness(m, pct, is_smooth, step, timeout);
    }
    return r;
}

static void set_each_brightness(sd_bus_message *m, double pct, const bool is_smooth, const double step, const int timeout) {
    const bool restoring = pct == 0;
    sensor_conf_t *sens_conf = &conf.sens_conf;
    enum ac_states st = state.ac_state;
    
    int r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "(sd)");
    while ((r = sd_bus_message_enter_container(m, SD_BUS_TYPE_STRUCT, "sd") >= 0) && r >= 0) {
        const char *mon_id = NULL;
        if (!restoring) {
            r = sd_bus_message_read(m, "sd", &mon_id, NULL);
        } else {
            r = sd_bus_message_read(m, "sd", &mon_id, &pct);
        }
        sd_bus_message_exit_container(m);
            
        if (r >= 0 && mon_id) {
            int ok = true;
                
            /* Set backlight on monitor id */
            SYSBUS_ARG_REPLY(args, parse_bus_reply, &ok, CLIGHTD_SERVICE, "/org/clightd/clightd/Backlight", "org.clightd.clightd.Backlight", "Set");
            curve_t *c = map_get(sens_conf->specific_curves, mon_id);
            if (c && !restoring) {
                /* Use monitor specific adjustment, properly scaling bl pct */
                const double real_pct = get_value_from_curve(pct, &c[st]);
                DEBUG("Using specific curve for '%s': setting %.3lf pct.\n", mon_id, real_pct);
                r = call(&args, "d(bdu)s", real_pct, is_smooth, step, timeout, mon_id);
            } else {
                DEBUG("Using default curve for '%s'\n", mon_id);
                /* Use non-adjusted (default) curve value */
                r = call(&args, "d(bdu)s", pct, is_smooth, step, timeout, mon_id);
            }
            if (!ok) {
                r = -1;
            }
        }
    }
    sd_bus_message_exit_container(m);
    sd_bus_message_unref(m);
}

static void set_backlight_level(const double pct, const bool is_smooth, const double step, const int timeout) {
    int r = -EINVAL;
    if (map_length(conf.sens_conf.specific_curves) > 0) {
        r = get_and_set_each_brightness(pct, is_smooth, step, timeout);
    } else {
        int ok = 0;
        SYSBUS_ARG_REPLY(args, parse_bus_reply, &ok, CLIGHTD_SERVICE, "/org/clightd/clightd/Backlight", "org.clightd.clightd.Backlight", "SetAll");
        
        /* Set backlight on both internal monitor (in case of laptop) and external ones */
        r = call(&args, "d(bdu)s", pct, is_smooth, step, timeout, conf.bl_conf.screen_path);
        if (!ok) {
            r = -1;
        }
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
static void reset_or_pause(int old_timeout) {
    const int new_timeout = get_current_timeout();
    if (new_timeout <= 0) {
        pause_mod(TIMEOUT);
    } else {
        resume_mod(TIMEOUT);
        reset_timer(bl_fd, old_timeout, new_timeout);
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
            
            reset_or_pause(old);
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
     reset_or_pause(old_timeout);
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
    
    if (!strcmp(backlight_interface, syspath)) {
        publish_bl_upd(pct, false, 0, 0);
        state.current_bl_pct = pct;
    }
    return 0;
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
    if (CHECK_PAUSE(true, type, "BACKLIGHT")) {
        m_become(paused);
        /* Properly deregister our fd while paused */
        m_deregister_fd(bl_fd);
    }
}

static void resume_mod(enum mod_pause type) {
    if (CHECK_PAUSE(false, type, "BACKLIGHT")) {
        m_unbecome();
        /* Register back our fd on resume */
        m_register_fd(bl_fd, false, NULL);
    }
}
