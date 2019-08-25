#include <stddef.h>
#include <bus.h>
#include <config.h>
#include <module/map.h>

#define CLIGHT_COOKIE -1
#define CLIGHT_INH_KEY "LockClight"

typedef struct {
    int cookie;
    int refs;
} lock_t;

/** org.freedesktop.ScreenSaver spec implementation **/
static int on_bus_name_changed(sd_bus_message *m, UNUSED void *userdata, UNUSED sd_bus_error *ret_error);
static int create_inhibit(int *cookie, const char *key, const char *app_name, const char *reason);
static int drop_inhibit(int *cookie, const char *key, bool force);
static int method_clight_inhibit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_inhibit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_uninhibit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_get_inhibit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

/** Clight bus api **/
static int get_version(sd_bus *b, const char *path, const char *interface, const char *property,
                       sd_bus_message *reply, void *userdata, sd_bus_error *error);
static int method_calibrate(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
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
static const char sc_interface[] = "org.freedesktop.ScreenSaver";

static const sd_bus_vtable clight_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("Version", "s", get_version, offsetof(state_t, version), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("ClightdVersion", "s", get_version, offsetof(state_t, clightd_version), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Sunrise", "t", NULL, offsetof(state_t, events[SUNRISE]), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Sunset", "t", NULL, offsetof(state_t, events[SUNSET]), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Time", "i", NULL, offsetof(state_t, time), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("InEvent", "b", NULL, offsetof(state_t, in_event), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("DisplayState", "i", NULL, offsetof(state_t, display_state), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("AcState", "i", NULL, offsetof(state_t, ac_state), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Inhibited", "b", NULL, offsetof(state_t, inhibited), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("BlPct", "d", NULL, offsetof(state_t, current_bl_pct), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("KbdPct", "d", NULL, offsetof(state_t, current_kbd_pct), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("AmbientBr", "d", NULL, offsetof(state_t, ambient_br), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Temp", "i", NULL, offsetof(state_t, current_temp), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Location", "(dd)", get_location, offsetof(state_t, current_loc), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("ScreenComp", "d", NULL, offsetof(state_t, screen_comp), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_METHOD("Calibrate", NULL, NULL, method_calibrate, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Inhibit", "b", NULL, method_clight_inhibit, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

static const sd_bus_vtable conf_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("NoBacklight", "b", NULL, offsetof(conf_t, no_backlight), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("NoGamma", "b", NULL, offsetof(conf_t, no_gamma), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("NoDimmer", "b", NULL, offsetof(conf_t, no_dimmer), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("NoDpms", "b", NULL, offsetof(conf_t, no_dpms), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("NoInhibit", "b", NULL, offsetof(conf_t, no_inhibit), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("NoScreen", "b", NULL, offsetof(conf_t, no_screen), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("ScreenSamples", "i", NULL, offsetof(conf_t, screen_samples), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_WRITABLE_PROPERTY("ScreenContrib", "d", NULL, set_screen_contrib, offsetof(conf_t, screen_contrib), 0),
    SD_BUS_WRITABLE_PROPERTY("Sunrise", "s", NULL, set_event, offsetof(conf_t, events[SUNRISE]), 0),
    SD_BUS_WRITABLE_PROPERTY("Sunset", "s", NULL, set_event, offsetof(conf_t, events[SUNSET]), 0),
    SD_BUS_WRITABLE_PROPERTY("Location", "(dd)", get_location, set_location, offsetof(conf_t, loc), 0),
    SD_BUS_WRITABLE_PROPERTY("NoAutoCalib", "b", NULL, set_auto_calib, offsetof(conf_t, no_auto_calib), 0),
    SD_BUS_WRITABLE_PROPERTY("NoKbdCalib", "b", NULL, NULL, offsetof(conf_t, no_keyboard_bl), 0),
    SD_BUS_WRITABLE_PROPERTY("AmbientGamma", "b", NULL, NULL, offsetof(conf_t, ambient_gamma), 0),
    SD_BUS_WRITABLE_PROPERTY("NoSmoothBacklight", "b", NULL, NULL, offsetof(conf_t, no_smooth_backlight), 0),
    SD_BUS_WRITABLE_PROPERTY("NoSmoothDimmerEnter", "b", NULL, NULL, offsetof(conf_t, no_smooth_dimmer[ENTER]), 0),
    SD_BUS_WRITABLE_PROPERTY("NoSmoothDimmerExit", "b", NULL, NULL, offsetof(conf_t, no_smooth_dimmer[EXIT]), 0),
    SD_BUS_WRITABLE_PROPERTY("NoSmoothGamma", "b", NULL, NULL, offsetof(conf_t, no_smooth_gamma), 0),
    SD_BUS_WRITABLE_PROPERTY("NumCaptures", "i", NULL, NULL, offsetof(conf_t, num_captures), 0),
    SD_BUS_WRITABLE_PROPERTY("SensorName", "s", NULL, NULL, offsetof(conf_t, dev_name), 0),
    SD_BUS_WRITABLE_PROPERTY("BacklightSyspath", "s", NULL, NULL, offsetof(conf_t, screen_path), 0),
    SD_BUS_WRITABLE_PROPERTY("EventDuration", "i", NULL, NULL, offsetof(conf_t, event_duration), 0),
    SD_BUS_WRITABLE_PROPERTY("DimmerPct", "d", NULL, NULL, offsetof(conf_t, dimmer_pct), 0),
    SD_BUS_WRITABLE_PROPERTY("Verbose", "b", NULL, NULL, offsetof(conf_t, verbose), 0),
    SD_BUS_WRITABLE_PROPERTY("BacklightTransStep", "d", NULL, NULL, offsetof(conf_t, backlight_trans_step), 0),
    SD_BUS_WRITABLE_PROPERTY("DimmerTransStepEnter", "d", NULL, NULL, offsetof(conf_t, dimmer_trans_step[ENTER]), 0),
    SD_BUS_WRITABLE_PROPERTY("DimmerTransStepExit", "d", NULL, NULL, offsetof(conf_t, dimmer_trans_step[EXIT]), 0),
    SD_BUS_WRITABLE_PROPERTY("GammaTransStep", "i", NULL, NULL, offsetof(conf_t, gamma_trans_step), 0),
    SD_BUS_WRITABLE_PROPERTY("BacklightTransDuration", "i", NULL, NULL, offsetof(conf_t, backlight_trans_timeout), 0),
    SD_BUS_WRITABLE_PROPERTY("GammaTransDuration", "i", NULL, NULL, offsetof(conf_t, gamma_trans_timeout), 0),
    SD_BUS_WRITABLE_PROPERTY("DimmerTransDurationEnter", "i", NULL, NULL, offsetof(conf_t, dimmer_trans_timeout[ENTER]), 0),
    SD_BUS_WRITABLE_PROPERTY("DimmerTransDurationExit", "i", NULL, NULL, offsetof(conf_t, dimmer_trans_timeout[EXIT]), 0),
    SD_BUS_WRITABLE_PROPERTY("DayTemp", "i", NULL, set_gamma, offsetof(conf_t, temp[DAY]), 0),
    SD_BUS_WRITABLE_PROPERTY("NightTemp", "i", NULL, set_gamma, offsetof(conf_t, temp[NIGHT]), 0),
    SD_BUS_WRITABLE_PROPERTY("AcCurvePoints", "ad", get_curve, set_curve, offsetof(conf_t, regression_points[ON_AC]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattCurvePoints", "ad", get_curve, set_curve, offsetof(conf_t, regression_points[ON_BATTERY]), 0),
    SD_BUS_WRITABLE_PROPERTY("ShutterThreshold", "d", NULL, NULL, offsetof(conf_t, shutter_threshold), 0),
    SD_BUS_WRITABLE_PROPERTY("GammaLongTransition", "b", NULL, NULL, offsetof(conf_t, gamma_long_transition), 0),
    SD_BUS_METHOD("Store", NULL, NULL, method_store_conf, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

static const sd_bus_vtable conf_to_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_WRITABLE_PROPERTY("AcDayCapture", "i", NULL, set_timeouts, offsetof(conf_t, timeout[ON_AC][DAY]), 0),
    SD_BUS_WRITABLE_PROPERTY("AcNightCapture", "i", NULL, set_timeouts, offsetof(conf_t, timeout[ON_AC][NIGHT]), 0),
    SD_BUS_WRITABLE_PROPERTY("AcEventCapture", "i", NULL, set_timeouts, offsetof(conf_t, timeout[ON_AC][IN_EVENT]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattDayCapture", "i", NULL, set_timeouts, offsetof(conf_t, timeout[ON_BATTERY][DAY]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattNightCapture", "i", NULL, set_timeouts, offsetof(conf_t, timeout[ON_BATTERY][NIGHT]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattEventCapture", "i", NULL, set_timeouts, offsetof(conf_t, timeout[ON_BATTERY][IN_EVENT]), 0),
    SD_BUS_WRITABLE_PROPERTY("AcDimmer", "i", NULL, set_timeouts, offsetof(conf_t, dimmer_timeout[ON_AC]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattDimmer", "i", NULL, set_timeouts, offsetof(conf_t, dimmer_timeout[ON_BATTERY]), 0),
    SD_BUS_WRITABLE_PROPERTY("AcDpms", "i", NULL, set_timeouts, offsetof(conf_t, dpms_timeout[ON_AC]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattDpms", "i", NULL, set_timeouts, offsetof(conf_t, dpms_timeout[ON_BATTERY]), 0),
    SD_BUS_WRITABLE_PROPERTY("AcScreen", "i", NULL, set_timeouts, offsetof(conf_t, screen_timeout[ON_AC]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattScreen", "i", NULL, set_timeouts, offsetof(conf_t, screen_timeout[ON_BATTERY]), 0),
    SD_BUS_VTABLE_END
};

static const sd_bus_vtable sc_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Inhibit", "ss", "u", method_inhibit, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("UnInhibit", "u", NULL, method_uninhibit, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD_WITH_OFFSET("GetActive", NULL, "b", method_get_inhibit, offsetof(state_t, inhibited), SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

static timeout_upd to_req;
static inhibit_upd inhibit_req = { INHIBIT_REQ };
static temp_upd temp_req = { TEMP_REQ };
static capture_upd capture_req = { CAPTURE_REQ };
static curve_upd curve_req = { CURVE_REQ };
static calib_upd calib_req = { AUTOCALIB_REQ };
static loc_upd loc_req = { LOCATION_REQ };
static loc_upd loc_msg = { LOCATION_UPD };
static contrib_upd contrib_req = { CONTRIB_REQ };

static map_t *lock_map;
static sd_bus *userbus;

MODULE("INTERFACE");

static void init(void) {    
    const char conf_path[] = "/org/clight/clight/Conf";
    const char conf_to_path[] = "/org/clight/clight/Conf/Timeouts";
    const char sc_path[] = "/org/freedesktop/ScreenSaver";
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
    
    sd_bus_add_object_vtable(userbus,
                                NULL,
                                sc_path,
                                sc_interface,
                                sc_vtable,
                                &state);
    
    if (r < 0) {
        WARN("Could not create %s dbus interface: %s\n", bus_interface, strerror(-r));
    } else {
        r = sd_bus_request_name(userbus, bus_interface, 0);
        if (r < 0) {
            WARN("Failed to create %s dbus interface: %s\n", bus_interface, strerror(-r));
        } else {
            /* Subscribe to any topic expept REQUESTS */
            m_subscribe("^[^Req].*");
            
            /** org.freedesktop.ScreenSaver API **/
            if (sd_bus_request_name(userbus, sc_interface, SD_BUS_NAME_REPLACE_EXISTING) < 0) {
                WARN("Failed to create %s dbus interface: %s\n", sc_interface, strerror(-r));
            } else {
                USERBUS_ARG(args, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "NameOwnerChanged");
                add_match(&args, NULL, on_bus_name_changed);
            }
            lock_map = map_new(true, free);
            /**                                 **/
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
        sd_bus_release_name(userbus, sc_interface);
        userbus = sd_bus_flush_close_unref(userbus);
    }
}

/** org.freedesktop.ScreenSaver spec implementation: https://people.freedesktop.org/~hadess/idle-inhibition-spec/re01.html **/

/*
 * org.freedesktop.ScreenSaver spec:
 * Inhibition will stop when the UnInhibit function is called, 
 * or the application disconnects from the D-Bus session bus (which usually happens upon exit).
 * 
 * Polling on NameOwnerChanged dbus signals.
 */
static int on_bus_name_changed(sd_bus_message *m, UNUSED void *userdata, UNUSED sd_bus_error *ret_error) {
    const char *name = NULL, *old_owner = NULL, *new_owner = NULL;
    if (sd_bus_message_read(m, "sss", &name, &old_owner, &new_owner) >= 0) {
        if (map_has_key(lock_map, old_owner) && (!new_owner || !strlen(new_owner))) {
            drop_inhibit(NULL, old_owner, true);
        }
    }
    return 0;
}

static int create_inhibit(int *cookie, const char *key, const char *app_name, const char *reason) {
    lock_t *l = map_get(lock_map, key);
    if (l) {
        l->refs++;
        *cookie = l->cookie;
    } else {
        lock_t *l = malloc(sizeof(lock_t));
        if (l) {
            if (*cookie != CLIGHT_COOKIE) {
                *cookie = random();
            }
            l->cookie = *cookie;
            l->refs = 1;
            map_put(lock_map, key, l);

            DEBUG("New ScreenSaver inhibition held by %s: %s. Cookie: %d\n", app_name, reason, l->cookie);

            if (map_length(lock_map) == 1) {
                inhibit_req.old = false;
                inhibit_req.new = true;
                M_PUB(&inhibit_req);
            }
        } else {
            return -1;
        }
    }
    return 0;
}

static int drop_inhibit(int *cookie, const char *key, bool force) {
    lock_t *l = map_get(lock_map, key);
    if (!l && cookie) {
        /* May be another sender is asking to drop a cookie? Linear search */
        for (map_itr_t *itr = map_itr_new(lock_map); itr; itr = map_itr_next(itr)) {
            lock_t *tmp = (lock_t *)map_itr_get_data(itr);
            if (tmp->cookie == *cookie) {
                l = tmp;
                free(itr);
                itr = NULL;
            }
        }
    }

    if (l) {
        const int c = l->cookie;
        if (!force) {
            l->refs--;
        } else {
            l->refs = 0;
        }
        if (l->refs == 0 && map_remove(lock_map, key) == MAP_OK) {
            DEBUG("Dropped ScreenSaver inhibition held by cookie: %d.\n", c);

            if (map_length(lock_map) == 0) {
                inhibit_req.old = true;
                inhibit_req.new = false;
                M_PUB(&inhibit_req);
            }
        }
        return 0;
    }
    return -1;
}

static int method_clight_inhibit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int inhibit;
    int r = sd_bus_message_read(m, "b", &inhibit);
    if (r < 0) {
        WARN("Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    int ret = 0;
    if (inhibit) {
        int cookie = CLIGHT_COOKIE;
        ret = create_inhibit(&cookie, CLIGHT_INH_KEY, "Clight", "user requested");
    } else {
        ret = drop_inhibit(NULL, CLIGHT_INH_KEY, true);
    }

    if (ret == 0) {
        return sd_bus_reply_method_return(m, NULL);
    }
    sd_bus_error_set_errno(ret_error, EINVAL);
    return -EINVAL;
}

static int method_inhibit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    char *app_name = NULL, *reason = NULL;
    
    int r = sd_bus_message_read(m, "ss", &app_name, &reason);
    if (r < 0) {
        WARN("Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    int cookie = 0;
    if (create_inhibit(&cookie, sd_bus_message_get_sender(m), app_name, reason) == 0) {
        return sd_bus_reply_method_return(m, "u", cookie);
    }
    sd_bus_error_set_errno(ret_error, ENOMEM);
    return -ENOMEM;
}

static int method_uninhibit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int cookie;
    
    int r = sd_bus_message_read(m, "u", &cookie);
    if (r < 0) {
        WARN("Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    if (drop_inhibit(&cookie, sd_bus_message_get_sender(m), false) == 0) {
        return sd_bus_reply_method_return(m, NULL);
    }
    sd_bus_error_set_errno(ret_error, EINVAL);
    return -EINVAL;
}

static int method_get_inhibit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    return sd_bus_reply_method_return(m, "b", *(bool *)userdata);
}

/** Clight bus api **/

static int get_version(sd_bus *b, const char *path, const char *interface, const char *property,
                       sd_bus_message *reply, void *userdata, sd_bus_error *error) {
    return sd_bus_message_append(reply, "s", userdata);
}
                       
static int method_calibrate(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    M_PUB(&capture_req);
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
        curve_req.state = ON_AC;
        if (userdata == conf.regression_points[ON_BATTERY]) {
            curve_req.state = ON_BATTERY;
        }
        memcpy(curve_req.regression_points, data, length);
        curve_req.num_points = length / sizeof(double);
        M_PUB(&curve_req);
    }
    return r;
}

static int get_location(sd_bus *bus, const char *path, const char *interface, const char *property,
                        sd_bus_message *reply, void *userdata, sd_bus_error *error) {
    loc_t *l = (loc_t *)userdata;
    return sd_bus_message_append(reply, "(dd)", l->lat, l->lon);
}

static int set_location(sd_bus *bus, const char *path, const char *interface, const char *property,
                        sd_bus_message *value, void *userdata, sd_bus_error *error) {
    loc_t *l = (loc_t *)userdata;
    loc_req.old = *l;
    sd_bus_message_read(value, "(dd)", &loc_req.new.lat, &loc_req.new.lon);
    
    if (fabs(loc_req.new.lat) <  90.0f && fabs(loc_req.new.lon) < 180.0f) {
        INFO("New location from BUS api: %.2lf %.2lf\n", loc_req.new.lat, loc_req.new.lat);
        
        /* Only if different from current one */
        if (state.current_loc.lat != loc_req.new.lat && state.current_loc.lon != loc_req.new.lon) {
            M_PUB(&loc_req);
        }
        return 0;
    }
    INFO("Wrong location set. Rejected.\n");
    return -EINVAL;
}

static int set_timeouts(sd_bus *bus, const char *path, const char *interface, const char *property,
                            sd_bus_message *value, void *userdata, sd_bus_error *error) {
    to_req.old = *(int *)userdata;
    int r = sd_bus_message_read(value, "i", &to_req.new);
    if (r < 0) {
        WARN("Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    /* Check if we modified currently used timeout! */
    to_req.type = -1;
    if (userdata == &conf.timeout[ON_AC][DAY]) {
        to_req.daytime = DAY;
        to_req.state = ON_AC;
        to_req.type = BL_TO_REQ;
    } else if (userdata == &conf.timeout[ON_AC][NIGHT]) {
        to_req.daytime = NIGHT;
        to_req.state = ON_AC;
        to_req.type = BL_TO_REQ;
    } else if (userdata == &conf.timeout[ON_AC][IN_EVENT]) {
        to_req.daytime = IN_EVENT;
        to_req.state = ON_AC;
        to_req.type = BL_TO_REQ;
    } else if (userdata == &conf.timeout[ON_BATTERY][DAY]) {
        to_req.daytime = DAY;
        to_req.state = ON_BATTERY;
        to_req.type = BL_TO_REQ;
    } else if (userdata == &conf.timeout[ON_BATTERY][NIGHT]) {
        to_req.daytime = NIGHT;
        to_req.state = ON_BATTERY;
        to_req.type = BL_TO_REQ;
    } else if (userdata == &conf.timeout[ON_BATTERY][IN_EVENT]) {
        to_req.daytime = IN_EVENT;
        to_req.state = ON_BATTERY;
        to_req.type = BL_TO_REQ;
    } else if (userdata == &conf.dimmer_timeout[ON_AC]) {
        to_req.type = DIMMER_TO_REQ;
        to_req.state = ON_AC;
    } else if (userdata == &conf.dimmer_timeout[ON_BATTERY]) {
        to_req.type = DIMMER_TO_REQ;
        to_req.state = ON_BATTERY;
    } else if (userdata == &conf.dpms_timeout[ON_AC]) {
        to_req.type = DPMS_TO_REQ;
        to_req.state = ON_AC;
    } else if (userdata == &conf.dpms_timeout[ON_BATTERY]) {
        to_req.type = DPMS_TO_REQ;
        to_req.state = ON_BATTERY;
    } else if (userdata == &conf.screen_timeout[ON_AC]) {
        to_req.type = SCR_TO_REQ;
        to_req.state = ON_AC;
    } else if (userdata == &conf.screen_timeout[ON_BATTERY]) {
        to_req.type = SCR_TO_REQ;
        to_req.state = ON_BATTERY;
    }
    
    if (to_req.type != -1) {
        M_PUB(&to_req);
    }
    return r;
}

static int set_gamma(sd_bus *bus, const char *path, const char *interface, const char *property,
                     sd_bus_message *value, void *userdata, sd_bus_error *error) {
    temp_req.old = *(int *)userdata;
    int r = sd_bus_message_read(value, "i", &temp_req.new);
    if (r >= 0 && temp_req.old != temp_req.new) {
        temp_req.smooth = -1; // use conf values
        M_PUB(&temp_req);
    }
    return r;
}

static int set_auto_calib(sd_bus *bus, const char *path, const char *interface, const char *property,
                          sd_bus_message *value, void *userdata, sd_bus_error *error) {
    calib_req.old = *(int *)userdata;
    int r = sd_bus_message_read(value, "b", &calib_req.new);    
    if (r >= 0 && calib_req.new != calib_req.old) {
        M_PUB(&calib_req);
    }
    return r;
}

static int set_event(sd_bus *bus, const char *path, const char *interface, const char *property,
                     sd_bus_message *value, void *userdata, sd_bus_error *error) {
    const char *event = NULL;
    int r = sd_bus_message_read(value, "s", &event);
    if (r >= 0 && strlen(event) <= sizeof(conf.events[SUNRISE])) {
        struct tm timeinfo;
        if (!strlen(event) || !strptime(event, "%R", &timeinfo)) {
            /* Failed to convert datetime */
            r = -EINVAL;
        } else {
            strncpy(userdata, event, sizeof(conf.events[SUNRISE]));
            M_PUB(&loc_msg); // only to let gamma know it should recompute state.events
        }
    } else {
        /* Datetime too long */
        r = -EINVAL;
    }
    return r;
}

static int set_screen_contrib(sd_bus *bus, const char *path, const char *interface, const char *property,
                     sd_bus_message *value, void *userdata, sd_bus_error *error) {
    contrib_req.old = *(double *)userdata;
    int r = sd_bus_message_read(value, "d", &contrib_req.new);
    if (r >= 0 && contrib_req.old != contrib_req.new) {
        M_PUB(&contrib_req);
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
