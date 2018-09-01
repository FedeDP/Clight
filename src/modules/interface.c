#include <bus.h>
#include <my_math.h>

static int get_version(sd_bus *b, const char *path, const char *interface, const char *property,
                        sd_bus_message *reply, void *userdata, sd_bus_error *error);
static int method_calibrate(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_manage(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_update_curve(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

static const char object_path[] = "/org/clight/clight";
static const char bus_interface[] = "org.clight.clight";
static const sd_bus_vtable clight_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("version", "s", get_version, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_METHOD("calibrate", NULL, NULL, method_calibrate, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("manage_module", "uu", "b", method_manage, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("set_backlight_curve", "uad", NULL, method_update_curve, SD_BUS_VTABLE_UNPRIVILEGED),
    /* TODO -> add methods to:
     * update timeouts
     * update gamma temperatures + re-apply correct gamma temperature (eg: you change daily temp to 6200, theb re-apply it)
     * query timeouts
     * query current settings (eg gamma temperatures)
     */
    SD_BUS_VTABLE_END
};


static struct dependency dependencies[] = { {HARD, USERBUS}, {SOFT, BRIGHTNESS} };
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
                                 NULL);
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

static int method_manage(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    enum modules mod;
    enum module_op op;
    
    /* Read the parameters */
    int r = sd_bus_message_read(m, "uu", &mod, &op);
    if (r < 0) {
        WARN("Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    r = -EINVAL;
    if (mod >= MODULES_NUM || op >= OP_NUM || !is_functional(mod)) {
        WARN("Wrong parameters.\n");
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Wrong parameters.");
    } else if ((op == PAUSE && is_paused(mod)) || (op == RESUME && !is_paused(mod))) {
        WARN("Module %s already in required state.\n", modules[mod].self->name);
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Module already in required state.");
    } else {
        r = manage_module(mod, op);
    }
    return sd_bus_reply_method_return(m, "b", r != -EINVAL && r != -1);
}

static int method_update_curve(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int r = -EINVAL;

    if (is_running(BRIGHTNESS)) {
        enum ac_states ac_state;
    
        /* Read the parameters */
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
