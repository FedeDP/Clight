#include <stddef.h>
#include <bus.h>
#include <config.h>

static int get_version(sd_bus *b, const char *path, const char *interface, const char *property,
                       sd_bus_message *reply, void *userdata, sd_bus_error *error);
static int method_calibrate(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_inhibit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int get_curve(sd_bus *bus, const char *path, const char *interface, const char *property,
                     sd_bus_message *reply, void *userdata, sd_bus_error *error);
static int set_curve(sd_bus *bus, const char *path, const char *interface, const char *property,
                    sd_bus_message *value, void *userdata, sd_bus_error *error);
static int get_location(sd_bus *bus, const char *path, const char *interface, const char *property,
                     sd_bus_message *reply, void *userdata, sd_bus_error *error);
static int set_location(sd_bus *bus, const char *path, const char *interface, const char *property,
                        sd_bus_message *value, void *userdata, sd_bus_error *error);
static int set_timeouts(sd_bus *bus, const char *path, const char *interface, const char *property,
                     sd_bus_message *value, void *userdata, sd_bus_error *error);
static int set_gamma(sd_bus *bus, const char *path, const char *interface, const char *property,
                     sd_bus_message *value, void *userdata, sd_bus_error *error);
static int set_auto_calib(sd_bus *bus, const char *path, const char *interface, const char *property,
                     sd_bus_message *value, void *userdata, sd_bus_error *error);
static int set_event(sd_bus *bus, const char *path, const char *interface, const char *property,
                        sd_bus_message *value, void *userdata, sd_bus_error *error);
static int set_screen_contrib(sd_bus *bus, const char *path, const char *interface, const char *property,
                              sd_bus_message *value, void *userdata, sd_bus_error *error);
static int method_store_conf(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

static const char object_path[] = "/org/clight/clight";
static const char bus_interface[] = "org.clight.clight";

static const sd_bus_vtable clight_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("Version", "s", get_version, offsetof(struct state, version), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("ClightdVersion", "s", get_version, offsetof(struct state, clightd_version), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Sunrise", "t", NULL, offsetof(struct state, events[SUNRISE]), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Sunset", "t", NULL, offsetof(struct state, events[SUNSET]), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Time", "i", NULL, offsetof(struct state, time), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("InEvent", "b", NULL, offsetof(struct state, in_event), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("DisplayState", "i", NULL, offsetof(struct state, display_state), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("AcState", "i", NULL, offsetof(struct state, ac_state), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("PmState", "i", NULL, offsetof(struct state, pm_inhibited), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("CurrentBlPct", "d", NULL, offsetof(struct state, current_bl_pct), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("CurrentKbdPct", "d", NULL, offsetof(struct state, current_kbd_pct), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("CurrentAmbientBr", "d", NULL, offsetof(struct state, ambient_br), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("CurrentTemp", "i", NULL, offsetof(struct state, current_temp), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Location", "(dd)", get_location, offsetof(struct state, current_loc), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("CurrentScreenComp", "d", NULL, offsetof(struct state, screen_comp), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_METHOD("Calibrate", NULL, NULL, method_calibrate, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Inhibit", "b", NULL, method_inhibit, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

static const sd_bus_vtable conf_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("NoBacklight", "b", NULL, offsetof(struct config, no_backlight), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("NoGamma", "b", NULL, offsetof(struct config, no_gamma), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("NoDimmer", "b", NULL, offsetof(struct config, no_dimmer), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("NoDpms", "b", NULL, offsetof(struct config, no_dpms), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("NoInhibit", "b", NULL, offsetof(struct config, no_inhibit), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("NoScreen", "b", NULL, offsetof(struct config, no_screen), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("ScreenSamples", "i", NULL, offsetof(struct config, screen_samples), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_WRITABLE_PROPERTY("ScreenContrib", "d", NULL, set_screen_contrib, offsetof(struct config, screen_contrib), 0),
    SD_BUS_WRITABLE_PROPERTY("Sunrise", "s", NULL, set_event, offsetof(struct config, events[SUNRISE]), 0),
    SD_BUS_WRITABLE_PROPERTY("Sunset", "s", NULL, set_event, offsetof(struct config, events[SUNSET]), 0),
    SD_BUS_WRITABLE_PROPERTY("Location", "(dd)", get_location, set_location, offsetof(struct config, loc), 0),
    SD_BUS_WRITABLE_PROPERTY("NoAutoCalib", "b", NULL, set_auto_calib, offsetof(struct config, no_auto_calib), 0),
    SD_BUS_WRITABLE_PROPERTY("NoKbdCalib", "b", NULL, NULL, offsetof(struct config, no_keyboard_bl), 0),
    SD_BUS_WRITABLE_PROPERTY("AmbientGamma", "b", NULL, NULL, offsetof(struct config, ambient_gamma), 0),
    SD_BUS_WRITABLE_PROPERTY("NoSmoothBacklight", "b", NULL, NULL, offsetof(struct config, no_smooth_backlight), 0),
    SD_BUS_WRITABLE_PROPERTY("NoSmoothDimmerEnter", "b", NULL, NULL, offsetof(struct config, no_smooth_dimmer[ENTER]), 0),
    SD_BUS_WRITABLE_PROPERTY("NoSmoothDimmerExit", "b", NULL, NULL, offsetof(struct config, no_smooth_dimmer[EXIT]), 0),
    SD_BUS_WRITABLE_PROPERTY("NoSmoothGamma", "b", NULL, NULL, offsetof(struct config, no_smooth_gamma), 0),
    SD_BUS_WRITABLE_PROPERTY("NumCaptures", "i", NULL, NULL, offsetof(struct config, num_captures), 0),
    SD_BUS_WRITABLE_PROPERTY("SensorName", "s", NULL, NULL, offsetof(struct config, dev_name), 0),
    SD_BUS_WRITABLE_PROPERTY("BacklightSyspath", "s", NULL, NULL, offsetof(struct config, screen_path), 0),
    SD_BUS_WRITABLE_PROPERTY("EventDuration", "i", NULL, NULL, offsetof(struct config, event_duration), 0),
    SD_BUS_WRITABLE_PROPERTY("DimmerPct", "d", NULL, NULL, offsetof(struct config, dimmer_pct), 0),
    SD_BUS_WRITABLE_PROPERTY("Verbose", "b", NULL, NULL, offsetof(struct config, verbose), 0),
    SD_BUS_WRITABLE_PROPERTY("BacklightTransStep", "d", NULL, NULL, offsetof(struct config, backlight_trans_step), 0),
    SD_BUS_WRITABLE_PROPERTY("DimmerTransStepEnter", "d", NULL, NULL, offsetof(struct config, dimmer_trans_step[ENTER]), 0),
    SD_BUS_WRITABLE_PROPERTY("DimmerTransStepExit", "d", NULL, NULL, offsetof(struct config, dimmer_trans_step[EXIT]), 0),
    SD_BUS_WRITABLE_PROPERTY("GammaTransStep", "i", NULL, NULL, offsetof(struct config, gamma_trans_step), 0),
    SD_BUS_WRITABLE_PROPERTY("BacklightTransDuration", "i", NULL, NULL, offsetof(struct config, backlight_trans_timeout), 0),
    SD_BUS_WRITABLE_PROPERTY("GammaTransDuration", "i", NULL, NULL, offsetof(struct config, gamma_trans_timeout), 0),
    SD_BUS_WRITABLE_PROPERTY("DimmerTransDurationEnter", "i", NULL, NULL, offsetof(struct config, dimmer_trans_timeout[ENTER]), 0),
    SD_BUS_WRITABLE_PROPERTY("DimmerTransDurationExit", "i", NULL, NULL, offsetof(struct config, dimmer_trans_timeout[EXIT]), 0),
    SD_BUS_WRITABLE_PROPERTY("DayTemp", "i", NULL, set_gamma, offsetof(struct config, temp[DAY]), 0),
    SD_BUS_WRITABLE_PROPERTY("NightTemp", "i", NULL, set_gamma, offsetof(struct config, temp[NIGHT]), 0),
    SD_BUS_WRITABLE_PROPERTY("AcCurvePoints", "ad", get_curve, set_curve, offsetof(struct config, regression_points[ON_AC]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattCurvePoints", "ad", get_curve, set_curve, offsetof(struct config, regression_points[ON_BATTERY]), 0),
    SD_BUS_WRITABLE_PROPERTY("ShutterThreshold", "d", NULL, NULL, offsetof(struct config, shutter_threshold), 0),
    SD_BUS_WRITABLE_PROPERTY("GammaLongTransition", "b", NULL, NULL, offsetof(struct config, gamma_long_transition), 0),
    SD_BUS_METHOD("Store", NULL, NULL, method_store_conf, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

static const sd_bus_vtable conf_to_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_WRITABLE_PROPERTY("AcDayCapture", "i", NULL, set_timeouts, offsetof(struct config, timeout[ON_AC][DAY]), 0),
    SD_BUS_WRITABLE_PROPERTY("AcNightCapture", "i", NULL, set_timeouts, offsetof(struct config, timeout[ON_AC][NIGHT]), 0),
    SD_BUS_WRITABLE_PROPERTY("AcEventCapture", "i", NULL, set_timeouts, offsetof(struct config, timeout[ON_AC][SIZE_STATES]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattDayCapture", "i", NULL, set_timeouts, offsetof(struct config, timeout[ON_BATTERY][DAY]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattNightCapture", "i", NULL, set_timeouts, offsetof(struct config, timeout[ON_BATTERY][NIGHT]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattEventCapture", "i", NULL, set_timeouts, offsetof(struct config, timeout[ON_BATTERY][SIZE_STATES]), 0),
    SD_BUS_WRITABLE_PROPERTY("AcDimmer", "i", NULL, set_timeouts, offsetof(struct config, dimmer_timeout[ON_AC]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattDimmer", "i", NULL, set_timeouts, offsetof(struct config, dimmer_timeout[ON_BATTERY]), 0),
    SD_BUS_WRITABLE_PROPERTY("AcDpms", "i", NULL, set_timeouts, offsetof(struct config, dpms_timeout[ON_AC]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattDpms", "i", NULL, set_timeouts, offsetof(struct config, dpms_timeout[ON_BATTERY]), 0),
    SD_BUS_WRITABLE_PROPERTY("AcScreen", "i", NULL, set_timeouts, offsetof(struct config, screen_timeout[ON_AC]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattScreen", "i", NULL, set_timeouts, offsetof(struct config, screen_timeout[ON_BATTERY]), 0),
    SD_BUS_VTABLE_END
};

static inhibit_upd inhibit_msg = { INHIBIT_UPD };
static timeout_upd to_msg = { TIMEOUT_UPD };
static temp_upd temp_msg = { INTERFACE_TEMP };
static capture_upd capture_msg = { DO_CAPTURE };
static curve_upd curve_msg = { CURVE_UPD };
static calib_upd calib_msg = { AUTOCALIB_UPD };
static loc_upd loc_msg = { LOCATION_UPD };
static contrib_upd contrib_msg = { CONTRIB_UPD };

const char *interface_temp_topic = "InterfaceTemp";
const char *interface_dimmer_to_topic = "InterfaceDimmerTo";
const char *interface_dpms_to_topic = "InterfaceDPMSTo";
const char *interface_scr_to_topic = "InterfaceScreenTO";
const char *interface_bl_to_topic = "InterfaceBLTo";
const char *interface_bl_capture = "InterfaceBLCapture";
const char *interface_bl_curve = "InterfaceBLCurve";
const char *interface_bl_autocalib = "InterfaceBLAuto";
const char *interface_scr_contrib = "InterfaceScrContrib";

static sd_bus *userbus;

MODULE("INTERFACE");

static void init(void) {    
    const char conf_path[] = "/org/clight/clight/Conf";
    const char conf_to_path[] = "/org/clight/clight/Conf/Timeouts";
    const char conf_interface[] = "org.clight.clight.Conf";

    userbus = get_user_bus();
    
    /* Main interface */
    int r = sd_bus_add_object_vtable(userbus,
                                NULL,
                                object_path,
                                bus_interface,
                                clight_vtable,
                                &state);

    /* Conf interface */
    r += sd_bus_add_object_vtable(userbus,
                                NULL,
                                conf_path,
                                conf_interface,
                                conf_vtable,
                                &conf);

    /* Conf/Timeouts interface */
    r += sd_bus_add_object_vtable(userbus,
                                  NULL,
                                  conf_to_path,
                                  conf_interface,
                                  conf_to_vtable,
                                  &conf);
    
    if (r < 0) {
        WARN("Could not create Bus Interface: %s\n", strerror(-r));
    } else {
        r = sd_bus_request_name(userbus, bus_interface, 0);
        if (r < 0) {
            WARN("Failed to acquire Bus Interface name: %s\n", strerror(-r));
        } else {
            /* Subscribe to any topic expept ours own */
            m_subscribe("^[^Interface].*");
        }
    }
    
    if (r < 0) {
        WARN("Failed to init.\n");
        m_poisonpill(self());
    }
}

static bool check(void) {
    return true;
}

static bool evaluate() {
    return true;
}

static void receive(const msg_t *const msg, const void* userdata) {
    if (msg->ps_msg->type == USER) {
        if (userbus) {
            DEBUG("Emitting %s property\n", msg->ps_msg->topic);
            sd_bus_emit_properties_changed(userbus, object_path, bus_interface, msg->ps_msg->topic, NULL);
        }
    }
}

static void destroy(void) {
    if (userbus) {
        sd_bus_release_name(userbus, bus_interface);
        userbus = sd_bus_flush_close_unref(userbus);
    }
}

static int get_version(sd_bus *b, const char *path, const char *interface, const char *property,
                       sd_bus_message *reply, void *userdata, sd_bus_error *error) {
    return sd_bus_message_append(reply, "s", userdata);
}

static int method_calibrate(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    m_publish(interface_bl_capture, &capture_msg, sizeof(capture_upd), false);
    return sd_bus_reply_method_return(m, NULL);
}

static int method_inhibit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int inhibited;
    int r = sd_bus_message_read(m, "b", &inhibited);
    if (r < 0) {
        WARN("Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }

    int old_inhibited = state.pm_inhibited;
    if (inhibited) {
        state.pm_inhibited |= PM_FORCED_ON;
    } else {
        state.pm_inhibited &= ~PM_FORCED_ON;
    }
    if (old_inhibited != state.pm_inhibited) {
        INFO("PowerManagement inhibition %s by bus API.\n", state.pm_inhibited ? "enabled" : "disabled");
        inhibit_msg.old = old_inhibited;
        inhibit_msg.new = state.pm_inhibited;
        M_PUB(inh_topic, &inhibit_msg);
    }
    return sd_bus_reply_method_return(m, NULL);
}

static int get_curve(sd_bus *bus, const char *path, const char *interface, const char *property,
                     sd_bus_message *reply, void *userdata, sd_bus_error *error) {
    
    enum ac_states st = ON_AC;
    if (userdata == conf.regression_points[ON_BATTERY]) {
        st = ON_BATTERY;
    }
    return sd_bus_message_append_array(reply, 'd', userdata, conf.num_points[st] * sizeof(double));
}

static int set_curve(sd_bus *bus, const char *path, const char *interface, const char *property,
                     sd_bus_message *value, void *userdata, sd_bus_error *error) {

    const double *data = NULL;
    size_t length;
    int r = sd_bus_message_read_array(value, 'd', (const void**) &data, &length);
    if (r < 0) {
        WARN("Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    if (length / sizeof(double) > MAX_SIZE_POINTS) {
        WARN("Wrong parameters.\n");
        sd_bus_error_set_const(error, SD_BUS_ERROR_FAILED, "Wrong parameters.");
        r = -EINVAL;
    } else {
        curve_msg.state = ON_AC;

        if (userdata == conf.regression_points[ON_BATTERY]) {
            curve_msg.state = ON_BATTERY;
        }
        memcpy(conf.regression_points[curve_msg.state], data, length);
        conf.num_points[curve_msg.state] = length / sizeof(double);
        m_publish(interface_bl_curve, &curve_msg, sizeof(curve_upd), false);
    }
    return r;
}

static int get_location(sd_bus *bus, const char *path, const char *interface, const char *property,
                        sd_bus_message *reply, void *userdata, sd_bus_error *error) {
    struct location *l = (struct location *)userdata;
    return sd_bus_message_append(reply, "(dd)", l->lat, l->lon);
}

static int set_location(sd_bus *bus, const char *path, const char *interface, const char *property,
                        sd_bus_message *value, void *userdata, sd_bus_error *error) {
    struct location *l = (struct location *)userdata;
    loc_msg.old = *l;
    sd_bus_message_read(value, "(dd)", &l->lat, &l->lon);
    
    if (fabs(l->lat) <  90.0f && fabs(l->lon) < 180.0f) {
        INFO("New location from BUS api: %.2lf %.2lf\n", l->lat, l->lon);
        
        /* Only if different from current one */
        if (state.current_loc.lat != l->lat && state.current_loc.lon != l->lon) {
            loc_msg.new = *l;
            memcpy(&state.current_loc, l, sizeof(struct location));
            M_PUB(loc_topic, &loc_msg);
        }
        return 0;
    }
    *l = loc_msg.old;
    INFO("Wrong location set. Rejected.\n");
    return -EINVAL;
}

static int set_timeouts(sd_bus *bus, const char *path, const char *interface, const char *property,
                            sd_bus_message *value, void *userdata, sd_bus_error *error) {
    int *val = (int *)userdata;
    int old_val = *val;

    int r = sd_bus_message_read(value, "i", userdata);
    if (r < 0) {
        WARN("Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    /* Check if we modified currently used timeout! */
    const char *topic = NULL;
    if (val == &conf.timeout[state.ac_state][state.time]) {
        topic = interface_bl_to_topic;
    } else if (val == &conf.dimmer_timeout[state.ac_state]) {
        topic = interface_dimmer_to_topic;
    } else if (val == &conf.dpms_timeout[state.ac_state]) {
        topic = interface_dpms_to_topic;
    } else if (val == &conf.screen_timeout[state.ac_state]) {
        topic = interface_scr_to_topic;
    }
    
    if (topic) {
        to_msg.old = old_val;
        to_msg.new = *val;
        M_PUB(topic, &to_msg);
    }
    return r;
}

static int set_gamma(sd_bus *bus, const char *path, const char *interface, const char *property,
                     sd_bus_message *value, void *userdata, sd_bus_error *error) {
    int old_val = *(int *)userdata;
    int r = sd_bus_message_read(value, "i", userdata);
    if (r >= 0 && old_val != *(int *)userdata) {
        temp_msg.new = *(int *)userdata;
        temp_msg.old = old_val;
        M_PUB(interface_temp_topic, &temp_msg);
    }
    return r;
}

static int set_auto_calib(sd_bus *bus, const char *path, const char *interface, const char *property,
                          sd_bus_message *value, void *userdata, sd_bus_error *error) {
    int old_val = *(int *)userdata;
    int r = sd_bus_message_read(value, "b", userdata);    
    if (r >= 0 && old_val != *(int *)userdata) {
        INFO("Backlight autocalibration %s by bus API.\n", *(int *)userdata ? "disabled" : "enabled");
        calib_msg.old = old_val;
        calib_msg.new = *(int *)userdata;
        M_PUB(interface_bl_autocalib, &calib_msg);
    }
    return r;
}

static int set_event(sd_bus *bus, const char *path, const char *interface, const char *property,
                     sd_bus_message *value, void *userdata, sd_bus_error *error) {
    const char *event = NULL;
    int r = sd_bus_message_read(value, "s", &event);
    if (r >= 0 && strlen(event) <= sizeof(conf.events[SUNRISE])) {
        struct tm timeinfo;
        if (strlen(event) && !strptime(event, "%R", &timeinfo)) {
            /* Failed to convert datetime */
            r = -EINVAL;
        } else {
            strncpy(userdata, event, sizeof(conf.events[SUNRISE]));
            M_PUB(loc_topic, &loc_msg); // only to let gamma know it should recompute state.events
        }
    } else {
        /* Datetime too long*/
        r = -EINVAL;
    }
    return r;
}

static int set_screen_contrib(sd_bus *bus, const char *path, const char *interface, const char *property,
                     sd_bus_message *value, void *userdata, sd_bus_error *error) {
    const double old_val = *(double *)userdata;
    int r = sd_bus_message_read(value, "d", userdata);
    if (r >= 0 && old_val != *(double *)userdata) {
        contrib_msg.old = old_val;
        contrib_msg.new = *(double *)userdata;
        M_PUB(interface_scr_contrib, &contrib_msg);
    }
    return r;
}

static int method_store_conf(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int r = -1;
    if (store_config(LOCAL) == 0) {
        r = sd_bus_reply_method_return(m, NULL);
    } else {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Failed to store conf.");
    }
    return r;
}
