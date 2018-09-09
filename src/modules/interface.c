#include <bus.h>
#include <my_math.h>
#include <stddef.h>

static int get_version(sd_bus *b, const char *path, const char *interface, const char *property,
                        sd_bus_message *reply, void *userdata, sd_bus_error *error);
static int method_calibrate(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_inhibit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_update_curve(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_setgamma(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

static const char object_path[] = "/org/clight/clight";
static const char bus_interface[] = "org.clight.clight";
static const sd_bus_vtable clight_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("Version", "s", get_version, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Sunrise", "t", NULL, offsetof(struct state, events), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Sunset", "t", NULL, offsetof(struct state, events) + sizeof(time_t), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Time", "i", NULL, offsetof(struct state, time), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Dimmed", "b", NULL, offsetof(struct state, is_dimmed), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("CurrentBrPct", "d", NULL, offsetof(struct state, current_br_pct), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_METHOD("Calibrate", NULL, NULL, method_calibrate, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Inhibit", "b", NULL, method_inhibit, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("SetBacklightCurve", "uad", NULL, method_update_curve, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("SetGamma", "ui", NULL, method_setgamma, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};


static struct dependency dependencies[] = { {SUBMODULE, USERBUS} };
static struct self_t self = {
    .num_deps = SIZE(dependencies),
    .deps = dependencies,
    .standalone = 1
};

MODULE(INTERFACE);

static void init(void) {
    sd_bus **userbus = get_user_bus();
    int r = sd_bus_add_object_vtable(*userbus,
                                 NULL,
                                 object_path,
                                 bus_interface,
                                 clight_vtable,
                                 &state);
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
}

static int get_version(sd_bus *b, const char *path, const char *interface, const char *property,
                        sd_bus_message *reply, void *userdata, sd_bus_error *error) {
    return sd_bus_message_append(reply, "s", VERSION);
}

static int method_calibrate(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int r = -EINVAL;
    
    if (is_running(BRIGHTNESS) && !state.is_dimmed) {
        set_timeout(0, 1, main_p[BRIGHTNESS].fd, 0);
        r = sd_bus_reply_method_return(m, NULL);
    } else if (!state.is_dimmed) {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Brightness module is not running.");
    } else {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Screen is currently dimmed.");
    }
    return r;
}

static int method_inhibit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    FILL_MATCH_DATA(state.pm_inhibited);

    /* Read the parameters */
    int r = sd_bus_message_read(m, "b", &state.pm_inhibited);
    if (r < 0) {
        WARN("Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    INFO("PowerManagement inhibition %s by bus API.\n", state.pm_inhibited ? "enabled" : "disabled");
    return sd_bus_reply_method_return(m, NULL);
}

static int method_update_curve(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int r = -EINVAL;

    if (is_running(BRIGHTNESS)) {
        enum ac_states ac_state;
    
        r = sd_bus_message_read(m, "u", &ac_state);
        if (r < 0) {
            WARN("Failed to parse parameters: %s\n", strerror(-r));
            return r;
        }
    
        const double *data = NULL;
        size_t length;
        r = sd_bus_message_read_array(m, 'd', (const void**) &data, &length);
        if (r < 0) {
            WARN("Failed to parse parameters: %s\n", strerror(-r));
            return r;
        }

        if (ac_state >= SIZE_AC || length / sizeof(double) != SIZE_POINTS) {
            WARN("Wrong parameters.\n");
            sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Wrong parameters.");
        } else {
            polynomialfit(ac_state, data);
            r = sd_bus_reply_method_return(m, NULL);
        }
    } else {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Brightness module is not running.");
    }
    return r;
}

static int method_setgamma(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int r = -EINVAL;
    
    if (is_running(GAMMA)) {
        enum states target_state;
        int gamma_val;
        
        r = sd_bus_message_read(m, "ui", &target_state, &gamma_val);
        if (r < 0) {
            WARN("Failed to parse parameters: %s\n", strerror(-r));
            return r;
        }
    
        if (target_state >= SIZE_STATES || gamma_val < 1000 || gamma_val > 10000) {
            WARN("Wrong parameters.\n");
            sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Wrong parameters.");
        } else {
            /* Use current state */
            if (target_state == EVENT) {
                target_state = state.time;
            }
            conf.temp[target_state] = gamma_val;
            set_timeout(0, 1, main_p[GAMMA].fd, 0);
            r = sd_bus_reply_method_return(m, NULL);
        }
    } else {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Gamma module is not running.");
    }
    return r;
}

int emit_prop(const char *signal) {
    if (is_running((self.idx))) {
        sd_bus **userbus = get_user_bus();
        return sd_bus_emit_properties_changed(*userbus, object_path, bus_interface, signal, NULL);
    }
    return -1;
}

int add_interface_match(sd_bus_slot **slot, sd_bus_message_handler_t cb) {
    if (is_running((self.idx))) {
        struct bus_args args = { bus_interface, object_path, "org.freedesktop.DBus.Properties", "PropertiesChanged", USER};
        return add_match(&args, slot, cb);
    }
    return -1;
}
