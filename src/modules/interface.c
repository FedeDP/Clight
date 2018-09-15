#include <bus.h>
#include <my_math.h>
#include <stddef.h>
#include <interface.h>

static int get_version(sd_bus *b, const char *path, const char *interface, const char *property,
                        sd_bus_message *reply, void *userdata, sd_bus_error *error);
static int method_calibrate(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_inhibit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_update_curve(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_setgamma(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
void run_prop_callbacks(const char *prop);

struct prop_callback {
    int num_callbacks;
    struct prop_cb *callbacks;
};

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

static struct prop_callback _cb;
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
    if (_cb.callbacks) {
        free(_cb.callbacks);
    }
}

static int get_version(sd_bus *b, const char *path, const char *interface, const char *property,
                        sd_bus_message *reply, void *userdata, sd_bus_error *error) {
    return sd_bus_message_append(reply, "s", VERSION);
}

static int method_calibrate(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int r = -EINVAL;
    
    if (is_running(BRIGHTNESS)) {
        FILL_MATCH_DATA(state.current_br_pct); // useless data, unused
        r = sd_bus_reply_method_return(m, NULL);
    } else {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Screen is currently dimmed.");
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
    state.pm_inhibited = inhibited;
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
    
        if (target_state >= EVENT || gamma_val < 1000 || gamma_val > 10000) {
            WARN("Wrong parameters.\n");
            sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Wrong parameters.");
        } else {
            if (conf.temp[target_state] != gamma_val) {
                FILL_MATCH_DATA(state.time);
                conf.temp[target_state] = gamma_val;
            }
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
        sd_bus_emit_properties_changed(*userbus, object_path, bus_interface, signal, NULL);
        run_prop_callbacks(signal);
        return 0;
    }
    return -1;
}

int add_prop_callback(struct prop_cb *cb) {
    struct prop_cb *tmp = realloc(_cb.callbacks, sizeof(struct prop_cb) * (++_cb.num_callbacks));
    if (tmp) {
        _cb.callbacks = tmp;
        _cb.callbacks[_cb.num_callbacks - 1] = *cb;
    } else {
        free(_cb.callbacks);
        ERROR("%s\n", strerror(errno));
    }
    return 0;
}

void run_prop_callbacks(const char *prop) {
    for (int i = 0; i < _cb.num_callbacks; i++) {
        if (!strcmp(_cb.callbacks[i].name, prop)) {
            _cb.callbacks[i].cb();
        }
    }
}
