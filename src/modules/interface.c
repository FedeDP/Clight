#include <bus.h>
#include <stddef.h>
#include <interface.h>
#include <config.h>

static int build_modules_vtable(sd_bus *userbus);
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
static int set_timeouts(sd_bus *bus, const char *path, const char *interface, const char *property, 
                     sd_bus_message *value, void *userdata, sd_bus_error *error);
static int set_gamma(sd_bus *bus, const char *path, const char *interface, const char *property, 
                     sd_bus_message *value, void *userdata, sd_bus_error *error);
static int method_store_conf(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static void run_prop_callbacks(const char *prop);

struct prop_callback {
    int num_callbacks;
    struct prop_cb *callbacks;
};

static const char object_path[] = "/org/clight/clight";
static const char bus_interface[] = "org.clight.clight";

static const sd_bus_vtable clight_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("Version", "s", get_version, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Sunrise", "t", NULL, offsetof(struct state, events[SUNRISE]), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Sunset", "t", NULL, offsetof(struct state, events[SUNSET]), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Time", "i", NULL, offsetof(struct state, time), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Dimmed", "b", NULL, offsetof(struct state, is_dimmed), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("CurrentBlPct", "d", NULL, offsetof(struct state, current_bl_pct), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("CurrentAmbientBr", "d", NULL, offsetof(struct state, ambient_br), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Location", "(dd)", get_location, offsetof(struct state, current_loc), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_METHOD("Calibrate", NULL, NULL, method_calibrate, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Inhibit", "b", NULL, method_inhibit, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

static const sd_bus_vtable conf_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("NoAutoCalib", "b", NULL, offsetof(struct config, no_auto_calib), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("NoKbdCalib", "b", NULL, offsetof(struct config, no_keyboard_bl), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Location", "(dd)", get_location, offsetof(struct config, loc), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Sunrise", "s", NULL, offsetof(struct config, events[SUNRISE]), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Sunset", "s", NULL, offsetof(struct config, events[SUNSET]), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_WRITABLE_PROPERTY("NoSmoothBacklight", "b", NULL, NULL, offsetof(struct config, no_smooth_backlight), 0),
    SD_BUS_WRITABLE_PROPERTY("NoSmoothDimmer", "b", NULL, NULL, offsetof(struct config, no_smooth_dimmer), 0),
    SD_BUS_WRITABLE_PROPERTY("NoSmoothGamma", "b", NULL, NULL, offsetof(struct config, no_smooth_gamma), 0),
    SD_BUS_WRITABLE_PROPERTY("NumCaptures", "i", NULL, NULL, offsetof(struct config, num_captures), 0),
    SD_BUS_WRITABLE_PROPERTY("SensorName", "s", NULL, NULL, offsetof(struct config, dev_name), 0),
    SD_BUS_WRITABLE_PROPERTY("BacklightSyspath", "s", NULL, NULL, offsetof(struct config, screen_path), 0),
    SD_BUS_WRITABLE_PROPERTY("EventDuration", "i", NULL, NULL, offsetof(struct config, event_duration), 0),
    SD_BUS_WRITABLE_PROPERTY("DimmerPct", "d", NULL, NULL, offsetof(struct config, dimmer_pct), 0),
    SD_BUS_WRITABLE_PROPERTY("Verbose", "b", NULL, NULL, offsetof(struct config, verbose), 0),
    SD_BUS_WRITABLE_PROPERTY("BacklightTransStep", "d", NULL, NULL, offsetof(struct config, backlight_trans_step), 0),
    SD_BUS_WRITABLE_PROPERTY("DimmerTransStep", "d", NULL, NULL, offsetof(struct config, dimmer_trans_step), 0),
    SD_BUS_WRITABLE_PROPERTY("GammaTransStep", "i", NULL, NULL, offsetof(struct config, gamma_trans_step), 0),
    SD_BUS_WRITABLE_PROPERTY("BacklightTransDuration", "i", NULL, NULL, offsetof(struct config, backlight_trans_timeout), 0),
    SD_BUS_WRITABLE_PROPERTY("GammaTransDuration", "i", NULL, NULL, offsetof(struct config, gamma_trans_timeout), 0),
    SD_BUS_WRITABLE_PROPERTY("DimmerTransDuration", "i", NULL, NULL, offsetof(struct config, dimmer_trans_timeout), 0),
    SD_BUS_WRITABLE_PROPERTY("DayTemp", "i", NULL, set_gamma, offsetof(struct config, temp[DAY]), 0),
    SD_BUS_WRITABLE_PROPERTY("NightTemp", "i", NULL, set_gamma, offsetof(struct config, temp[NIGHT]), 0),
    SD_BUS_WRITABLE_PROPERTY("AcCurvePoints", "ad", get_curve, set_curve, offsetof(struct config, regression_points[ON_AC]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattCurvePoints", "ad", get_curve, set_curve, offsetof(struct config, regression_points[ON_BATTERY]), 0),
    SD_BUS_WRITABLE_PROPERTY("ShutterThreshold", "d", NULL, NULL, offsetof(struct config, shutter_threshold), 0),
    SD_BUS_METHOD("Store", NULL, NULL, method_store_conf, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

static const sd_bus_vtable conf_to_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_WRITABLE_PROPERTY("AcDayCapture", "i", NULL, set_timeouts, offsetof(struct config, timeout[ON_AC][DAY]), 0),
    SD_BUS_WRITABLE_PROPERTY("AcNightCapture", "i", NULL, set_timeouts, offsetof(struct config, timeout[ON_AC][NIGHT]), 0),
    SD_BUS_WRITABLE_PROPERTY("AcEventCapture", "i", NULL, set_timeouts, offsetof(struct config, timeout[ON_AC][EVENT]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattDayCapture", "i", NULL, set_timeouts, offsetof(struct config, timeout[ON_BATTERY][DAY]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattNightCapture", "i", NULL, set_timeouts, offsetof(struct config, timeout[ON_BATTERY][NIGHT]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattEventCapture", "i", NULL, set_timeouts, offsetof(struct config, timeout[ON_BATTERY][EVENT]), 0),
    SD_BUS_WRITABLE_PROPERTY("AcDimmer", "i", NULL, set_timeouts, offsetof(struct config, dimmer_timeout[ON_AC]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattDimmer", "i", NULL, set_timeouts, offsetof(struct config, dimmer_timeout[ON_BATTERY]), 0),
    SD_BUS_WRITABLE_PROPERTY("AcDpmsStandby", "i", NULL, set_timeouts, offsetof(struct config, dpms_timeouts[ON_AC][STANDBY]), 0),
    SD_BUS_WRITABLE_PROPERTY("AcDpmsSuspend", "i", NULL, set_timeouts, offsetof(struct config, dpms_timeouts[ON_AC][SUSPEND]), 0),
    SD_BUS_WRITABLE_PROPERTY("AcDpmsOff", "i", NULL, set_timeouts, offsetof(struct config, dpms_timeouts[ON_AC][OFF]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattDpmsStandby", "i", NULL, set_timeouts, offsetof(struct config, dpms_timeouts[ON_BATTERY][STANDBY]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattDpmsSuspend", "i", NULL, set_timeouts, offsetof(struct config, dpms_timeouts[ON_BATTERY][SUSPEND]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattDpmsOff", "i", NULL, set_timeouts, offsetof(struct config, dpms_timeouts[ON_BATTERY][OFF]), 0),
    SD_BUS_VTABLE_END
};

static sd_bus_vtable module_vtable[MODULES_NUM + 2];

static struct prop_callback _cb;
static struct dependency dependencies[] = { 
    {SUBMODULE, USERBUS}    // It must be started together with userbus
};
static struct self_t self = {
    .num_deps = SIZE(dependencies),
    .deps = dependencies,
    .standalone = 1
};

MODULE(INTERFACE);

static void init(void) {
    const char conf_path[] = "/org/clight/clight/Conf";
    const char conf_to_path[] = "/org/clight/clight/Conf/Timeouts";
    const char conf_interface[] = "org.clight.clight.Conf";

    sd_bus **userbus = get_user_bus();
    /* Main interface */
    int r = sd_bus_add_object_vtable(*userbus,
                                NULL,
                                object_path,
                                bus_interface,
                                clight_vtable,
                                &state);

    /* Conf interface */
    r += sd_bus_add_object_vtable(*userbus,
                                NULL,
                                conf_path,
                                conf_interface,
                                conf_vtable,
                                &conf);

    /* Conf/Timeouts interface */
    r += sd_bus_add_object_vtable(*userbus,
                                  NULL,
                                  conf_to_path,
                                  conf_interface,
                                  conf_to_vtable,
                                  &conf);

    /* Modules interface */
    r += build_modules_vtable(*userbus);
    if (r < 0) {
        WARN("Could not create Bus Interface: %s\n", strerror(-r));
    } else {
        r = sd_bus_request_name(*userbus, bus_interface, 0);
        if (r < 0) {
            WARN("Failed to acquire Bus Interface name: %s\n", strerror(-r));
        }
    }

    /* In case of errors, disable interface. */
    INIT_MOD(r >= 0 ? DONT_POLL : DONT_POLL_W_ERR);
}

static int check(void) {
    return 0;
}

static void callback(void) {
    // Skeleton interface
}

static void destroy(void) {
    sd_bus **userbus = get_user_bus();
    sd_bus_release_name(*userbus, bus_interface);
    if (_cb.callbacks) {
        free(_cb.callbacks);
    }
}

static int build_modules_vtable(sd_bus *userbus) {
    const char module_path[] = "/org/clight/clight/Modules";
    const char module_interface[] = "org.clight.clight.Modules";

    int i = 0;
    module_vtable[i] = (sd_bus_vtable)SD_BUS_VTABLE_START(0);
    for (i = 1; i <= MODULES_NUM; i++) {
        module_vtable[i] = (sd_bus_vtable) SD_BUS_PROPERTY(modules[i - 1].self->name, "u", NULL, 
                                                           offsetof(struct module, state) + (sizeof(struct module) * (i - 1)), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE);
    }
    module_vtable[i] = (sd_bus_vtable)SD_BUS_VTABLE_END;
    return sd_bus_add_object_vtable(userbus,
                                  NULL,
                                  module_path,
                                  module_interface,
                                  module_vtable,
                                  modules);
}

static int get_version(sd_bus *b, const char *path, const char *interface, const char *property,
                        sd_bus_message *reply, void *userdata, sd_bus_error *error) {
    return sd_bus_message_append(reply, "s", VERSION);
}

static int method_calibrate(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int r = -EINVAL;

    if (is_running(BACKLIGHT)) {
        FILL_MATCH_DATA(state.current_bl_pct); // useless data, unused
        r = sd_bus_reply_method_return(m, NULL);
    } else {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Backlight module is not running.");
    }
    return r;
}

static int method_inhibit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int inhibited;
    int r = sd_bus_message_read(m, "b", &inhibited);
    if (r < 0) {
        WARN("Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }

    FILL_MATCH_DATA(state.pm_inhibited);
    state.pm_inhibited = inhibited ? PM_FORCED_ON : PM_OFF;
    INFO("PowerManagement inhibition %s by bus API.\n", state.pm_inhibited ? "enabled" : "disabled");
    return sd_bus_reply_method_return(m, NULL);
}

static int get_curve(sd_bus *bus, const char *path, const char *interface, const char *property, 
                     sd_bus_message *reply, void *userdata, sd_bus_error *error) {
    return sd_bus_message_append_array(reply, 'd', userdata, SIZE_POINTS * sizeof(double));
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
    if (length / sizeof(double) != SIZE_POINTS) {
        WARN("Wrong parameters.\n");
        sd_bus_error_set_const(error, SD_BUS_ERROR_FAILED, "Wrong parameters.");
        r = -EINVAL;
    } else {
        enum ac_states ac_state = ON_AC;

        if (userdata == conf.regression_points[ON_BATTERY]) {
            ac_state = ON_BATTERY;
        }
        memcpy(conf.regression_points[ac_state], data, length);
        FILL_MATCH_DATA(ac_state);
    }
    return r;
}

static int get_location(sd_bus *bus, const char *path, const char *interface, const char *property, 
                        sd_bus_message *reply, void *userdata, sd_bus_error *error) {
    struct location *l = (struct location *)userdata;
    return sd_bus_message_append(reply, "(dd)", l->lat, l->lon);
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
    if (val == &conf.timeout[state.ac_state][state.time]) {
        FILL_MATCH_DATA_NAME(old_val, "backlight_timeout");
    } else if (val == &conf.dimmer_timeout[state.ac_state]) {
        FILL_MATCH_DATA_NAME(old_val, "dimmer_timeout");
    } else if (val >= conf.dpms_timeouts[state.ac_state] && val <= &conf.dpms_timeouts[state.ac_state][SIZE_DPMS - 1]) {
        FILL_MATCH_DATA_NAME(old_val, "dpms_timeout");
    }
    return r;
}

static int set_gamma(sd_bus *bus, const char *path, const char *interface, const char *property, 
                     sd_bus_message *value, void *userdata, sd_bus_error *error) {    
    int r = sd_bus_message_read(value, "i", userdata);
    FILL_MATCH_DATA(state.time); // useless data, unused
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

int emit_prop(const char *signal) {
    if (is_running((self.idx))) {
        sd_bus **userbus = get_user_bus();
        if (*userbus) {
            sd_bus_emit_properties_changed(*userbus, object_path, bus_interface, signal, NULL);
            run_prop_callbacks(signal);
            return 0;
        }
    }
    return -1;
}

int add_prop_callback(struct prop_cb *cb) {
    if (is_running((self.idx))) {
        struct prop_cb *tmp = realloc(_cb.callbacks, sizeof(struct prop_cb) * (++_cb.num_callbacks));
        if (tmp) {
            _cb.callbacks = tmp;
            _cb.callbacks[_cb.num_callbacks - 1] = *cb;
        } else {
            free(_cb.callbacks);
            ERROR("%s\n", strerror(errno));
        }
    }
    return 0;
}

static void run_prop_callbacks(const char *prop) {
    for (int i = 0; i < _cb.num_callbacks; i++) {
        if (!strcmp(_cb.callbacks[i].name, prop)) {
            _cb.callbacks[i].cb();
        }
    }
}
