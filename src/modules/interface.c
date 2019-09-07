#include <module/map.h>
#include "bus.h"
#include "config.h"

#define VALIDATE_PARAMS(m, signature, ...) \
    int r = sd_bus_message_read(m, signature, __VA_ARGS__); \
    if (r < 0) { \
        WARN("Failed to parse parameters: %s\n", strerror(-r)); \
        return r; \
    }

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
static int method_load(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_unload(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
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
    SD_BUS_PROPERTY("Sunrise", "t", NULL, offsetof(state_t, day_events[SUNRISE]), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Sunset", "t", NULL, offsetof(state_t, day_events[SUNSET]), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("DayTime", "i", NULL, offsetof(state_t, day_time), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
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
    SD_BUS_METHOD("Load", "s", NULL, method_load, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Unload", "s", NULL, method_unload, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

static const sd_bus_vtable conf_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("NoBacklight", "b", NULL, offsetof(conf_t, no_backlight), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("NoGamma", "b", NULL, offsetof(conf_t, no_gamma), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("NoDimmer", "b", NULL, offsetof(conf_t, no_dimmer), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("NoDpms", "b", NULL, offsetof(conf_t, no_dpms), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("NoScreen", "b", NULL, offsetof(conf_t, no_screen), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("ScreenSamples", "i", NULL, offsetof(conf_t, screen_samples), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_WRITABLE_PROPERTY("ScreenContrib", "d", NULL, set_screen_contrib, offsetof(conf_t, screen_contrib), 0),
    SD_BUS_WRITABLE_PROPERTY("Sunrise", "s", NULL, set_event, offsetof(conf_t, day_events[SUNRISE]), 0),
    SD_BUS_WRITABLE_PROPERTY("Sunset", "s", NULL, set_event, offsetof(conf_t, day_events[SUNSET]), 0),
    SD_BUS_WRITABLE_PROPERTY("Location", "(dd)", get_location, set_location, offsetof(conf_t, loc), 0),
    SD_BUS_WRITABLE_PROPERTY("NoAutoCalib", "b", NULL, set_auto_calib, offsetof(conf_t, no_auto_calib), 0),
    SD_BUS_WRITABLE_PROPERTY("InhibitAutoCalib", "b", NULL, NULL, offsetof(conf_t, inhibit_autocalib), 0),
    SD_BUS_WRITABLE_PROPERTY("NoKbdCalib", "b", NULL, NULL, offsetof(conf_t, no_keyboard_bl), 0),
    SD_BUS_WRITABLE_PROPERTY("AmbientGamma", "b", NULL, NULL, offsetof(conf_t, ambient_gamma), 0),
    SD_BUS_WRITABLE_PROPERTY("NoSmoothBacklight", "b", NULL, NULL, offsetof(conf_t, no_smooth_backlight), 0),
    SD_BUS_WRITABLE_PROPERTY("NoSmoothDimmerEnter", "b", NULL, NULL, offsetof(conf_t, no_smooth_dimmer[ENTER]), 0),
    SD_BUS_WRITABLE_PROPERTY("NoSmoothDimmerExit", "b", NULL, NULL, offsetof(conf_t, no_smooth_dimmer[EXIT]), 0),
    SD_BUS_WRITABLE_PROPERTY("NoSmoothGamma", "b", NULL, NULL, offsetof(conf_t, no_smooth_gamma), 0),
    SD_BUS_WRITABLE_PROPERTY("NumCaptures", "i", NULL, NULL, offsetof(conf_t, num_captures), 0),
    SD_BUS_WRITABLE_PROPERTY("SensorName", "s", NULL, NULL, offsetof(conf_t, dev_name), 0),
    SD_BUS_WRITABLE_PROPERTY("SensorSettings", "s", NULL, NULL, offsetof(conf_t, dev_opts), 0),
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

DECLARE_MSG(bl_to_req, BL_TO_REQ);
DECLARE_MSG(dimmer_to_req, DIMMER_TO_REQ);
DECLARE_MSG(dpms_to_req, DPMS_TO_REQ);
DECLARE_MSG(scr_to_req, SCR_TO_REQ);
DECLARE_MSG(inhibit_req, INHIBIT_REQ);
DECLARE_MSG(temp_req, TEMP_REQ);
DECLARE_MSG(capture_req, CAPTURE_REQ);
DECLARE_MSG(curve_req, CURVE_REQ);
DECLARE_MSG(calib_req, NO_AUTOCALIB_REQ);
DECLARE_MSG(loc_req, LOCATION_REQ);
DECLARE_MSG(contrib_req, CONTRIB_REQ);
DECLARE_MSG(sunrise_req, SUNRISE_REQ);
DECLARE_MSG(sunset_req, SUNSET_REQ);

static map_t *lock_map;
static sd_bus *userbus;
// this is used to keep curve points data lingering around in set_curve
static sd_bus_message *curve_message;

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

static void receive(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
    case FD_UPD:
        if (userbus) {
            DEBUG("Emitting %s property\n", msg->ps_msg->topic);
            sd_bus_emit_properties_changed(userbus, object_path, bus_interface, msg->ps_msg->topic, NULL);
        }
        break;
    default:
        break;
    }
}

static void destroy(void) {
    if (userbus) {
        sd_bus_release_name(userbus, bus_interface);
        sd_bus_release_name(userbus, sc_interface);
        userbus = sd_bus_flush_close_unref(userbus);
    }
    map_free(lock_map);
    curve_message = sd_bus_message_unref(curve_message);
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
                inhibit_req.inhibit.old = false;
                inhibit_req.inhibit.new = true;
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
                inhibit_req.inhibit.old = true;
                inhibit_req.inhibit.new = false;
                M_PUB(&inhibit_req);
            }
        }
        return 0;
    }
    return -1;
}

static int method_clight_inhibit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int inhibit;
    VALIDATE_PARAMS(m, "b", &inhibit);
    
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
    
    VALIDATE_PARAMS(m, "ss", &app_name, &reason);
    
    int cookie = 0;
    if (create_inhibit(&cookie, sd_bus_message_get_sender(m), app_name, reason) == 0) {
        return sd_bus_reply_method_return(m, "u", cookie);
    }
    sd_bus_error_set_errno(ret_error, ENOMEM);
    return -ENOMEM;
}

static int method_uninhibit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int cookie;
    
    VALIDATE_PARAMS(m, "u", &cookie);
    
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
    capture_req.capture.reset_timer = false;
    M_PUB(&capture_req);
    return sd_bus_reply_method_return(m, NULL);
}

static int method_load(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    const char *module_path;

    VALIDATE_PARAMS(m, "s", &module_path);
    if (m_load(module_path) == MOD_OK) {
        INFO("'%s' loaded.\n", module_path);
        return sd_bus_reply_method_return(m, NULL);
    }

    WARN("'%s' failed to load.\n", module_path);
    sd_bus_error_set_errno(ret_error, EINVAL);
    return -EINVAL;
}

static int method_unload(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    const char *module_path;

    VALIDATE_PARAMS(m, "s", &module_path);
    if (m_unload(module_path) == MOD_OK) {
        INFO("'%s' unloaded.\n", module_path);
        return sd_bus_reply_method_return(m, NULL);
    }

    WARN("'%s' failed to unload.\n", module_path);
    sd_bus_error_set_errno(ret_error, EINVAL);
    return -EINVAL;
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

    /* Unref last curve message, if any */
    curve_message = sd_bus_message_unref(curve_message);

    double *data = NULL;
    size_t length;
    int r = sd_bus_message_read_array(value, 'd', (const void**) &data, &length);
    if (r < 0) {
        WARN("Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    curve_req.curve.num_points = length / sizeof(double);
    if (curve_req.curve.num_points > MAX_SIZE_POINTS) {
        WARN("Wrong parameters.\n");
        sd_bus_error_set_const(error, SD_BUS_ERROR_FAILED, "Wrong parameters.");
        r = -EINVAL;
    } else {
        curve_req.curve.state = ON_AC;
        if (userdata == conf.regression_points[ON_BATTERY]) {
            curve_req.curve.state = ON_BATTERY;
        }
        curve_req.curve.regression_points = data;
        curve_message = sd_bus_message_ref(value);
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

    VALIDATE_PARAMS(value, "(dd)", &loc_req.loc.new.lat, &loc_req.loc.new.lon);

    INFO("New location from BUS api: %.2lf %.2lf\n", loc_req.loc.new.lat, loc_req.loc.new.lat);
    M_PUB(&loc_req);
    return r;
}

static int set_timeouts(sd_bus *bus, const char *path, const char *interface, const char *property,
                            sd_bus_message *value, void *userdata, sd_bus_error *error) {    
    /* Check if we modified currently used timeout! */
    message_t *msg = NULL;
    if (userdata == &conf.timeout[ON_AC][DAY]) {
        msg = &bl_to_req;
        bl_to_req.to.daytime = DAY;
        bl_to_req.to.state = ON_AC;
    } else if (userdata == &conf.timeout[ON_AC][NIGHT]) {
        msg = &bl_to_req;
        bl_to_req.to.daytime = NIGHT;
        bl_to_req.to.state = ON_AC;
    } else if (userdata == &conf.timeout[ON_AC][IN_EVENT]) {
        msg = &bl_to_req;
        bl_to_req.to.daytime = IN_EVENT;
        bl_to_req.to.state = ON_AC;
    } else if (userdata == &conf.timeout[ON_BATTERY][DAY]) {
        msg = &bl_to_req;
        bl_to_req.to.daytime = DAY;
        bl_to_req.to.state = ON_BATTERY;
    } else if (userdata == &conf.timeout[ON_BATTERY][NIGHT]) {
        msg = &bl_to_req;
        bl_to_req.to.daytime = NIGHT;
        bl_to_req.to.state = ON_BATTERY;
    } else if (userdata == &conf.timeout[ON_BATTERY][IN_EVENT]) {
        msg = &bl_to_req;
        bl_to_req.to.daytime = IN_EVENT;
        bl_to_req.to.state = ON_BATTERY;
    } else if (userdata == &conf.dimmer_timeout[ON_AC]) {
        msg = &dimmer_to_req;
        dimmer_to_req.to.state = ON_AC;
    } else if (userdata == &conf.dimmer_timeout[ON_BATTERY]) {
        msg = &dimmer_to_req;
        dimmer_to_req.to.state = ON_BATTERY;
    } else if (userdata == &conf.dpms_timeout[ON_AC]) {
        msg = &dpms_to_req;
        dpms_to_req.to.state = ON_AC;
    } else if (userdata == &conf.dpms_timeout[ON_BATTERY]) {
        msg = &dpms_to_req;
        dimmer_to_req.to.state = ON_BATTERY;
    } else if (userdata == &conf.screen_timeout[ON_AC]) {
        msg = &scr_to_req;
        scr_to_req.to.state = ON_AC;
    } else if (userdata == &conf.screen_timeout[ON_BATTERY]) {
        msg = &scr_to_req;
        scr_to_req.to.state = ON_BATTERY;
    }
    
    VALIDATE_PARAMS(value, "i", &msg->to.new);

    if (msg) {
        M_PUB(msg);
    }
    return r;
}

static int set_gamma(sd_bus *bus, const char *path, const char *interface, const char *property,
                     sd_bus_message *value, void *userdata, sd_bus_error *error) {
    VALIDATE_PARAMS(value, "i", &temp_req.temp.new);
    
    temp_req.temp.daytime = userdata == &conf.temp[DAY] ? DAY : NIGHT;
    temp_req.temp.smooth = -1; // use conf values
    M_PUB(&temp_req);
    return r;
}

static int set_auto_calib(sd_bus *bus, const char *path, const char *interface, const char *property,
                          sd_bus_message *value, void *userdata, sd_bus_error *error) {
    VALIDATE_PARAMS(value, "b", &calib_req.nocalib.new);
    
    M_PUB(&calib_req);
    return r;
}

static int set_event(sd_bus *bus, const char *path, const char *interface, const char *property,
                     sd_bus_message *value, void *userdata, sd_bus_error *error) {
    const char *event = NULL;
    VALIDATE_PARAMS(value, "s", &event);

    message_t *msg = &sunrise_req;
    if (userdata == &conf.day_events[SUNSET]) {
        msg = &sunset_req;
    }
    strncpy(msg->event.event, event, sizeof(msg->event.event));
    M_PUB(msg);
    return r;
}

static int set_screen_contrib(sd_bus *bus, const char *path, const char *interface, const char *property,
                     sd_bus_message *value, void *userdata, sd_bus_error *error) {
    VALIDATE_PARAMS(value, "d", &contrib_req.contrib.new);

    M_PUB(&contrib_req);
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
