#include <bus.h>
#include <my_math.h>

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
    SD_BUS_PROPERTY("version", "s", get_version, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_METHOD("calibrate", NULL, NULL, method_calibrate, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("inhibit", "b", NULL, method_inhibit, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("set_backlight_curve", "uad", NULL, method_update_curve, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("set_gamma", "ui", NULL, method_setgamma, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};


static struct dependency dependencies[] = { {HARD, USERBUS}, {SOFT, BRIGHTNESS}, {SOFT, GAMMA} };
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
                                 get_user_data());
    if (r < 0) {
        WARN("Failed to issue method call: %s\n", strerror(-r));
    } else {
        r = sd_bus_request_name(*userbus, bus_interface, 0);
        if (r < 0) {
            WARN("Failed to acquire service name: %s\n", strerror(-r));
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
    int r;
    
    if (is_running(BRIGHTNESS)) {
        set_timeout(0, 1, main_p[BRIGHTNESS].fd, 0);
        r = sd_bus_reply_method_return(m, NULL);
    } else {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Brightness module is not running.");
        r = -EINVAL;
    }
    return r;
}

static int method_inhibit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    struct bus_match_data *data = (struct bus_match_data *) userdata;
    data->bus_mod_idx = self.idx;
    /* Fill data->ptr with old inhibit state */
    data->ptr = malloc(sizeof(int));
    *(int *)(data->ptr) = state.pm_inhibited;
    
    /* Read the parameters */
    int r = sd_bus_message_read(m, "b", &state.pm_inhibited);
    if (r < 0) {
        WARN("Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }

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
            memcpy(conf.regression_points[ac_state], data, length);
            polynomialfit(ac_state);
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
