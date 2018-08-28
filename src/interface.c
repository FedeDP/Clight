#include "../inc/interface.h"
#include "../inc/bus.h"

static void init(void);
static int check(void);
static void callback(void);
static void destroy(void);
static int get_version( sd_bus *b, const char *path, const char *interface, const char *property,
                        sd_bus_message *reply, void *userdata, sd_bus_error *error);
static int method_calibrate(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

static const char object_path[] = "/org/clight/clight";
static const char bus_interface[] = "org.clight.clight";
static const sd_bus_vtable clight_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("version", "s", get_version, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_METHOD("calibrate", NULL, NULL, method_calibrate, SD_BUS_VTABLE_UNPRIVILEGED),
    /* TODO: add methods to: 
     * update timeouts
     * update gamma temperatures + re-apply correct gamma temperature (eg: you change daily temp to 6200, theb re-apply it)
     * query timeouts
     * query current settings (eg gamma temperatures)
     * change backlight curve points
     * start/pause functional modules
     */
    SD_BUS_VTABLE_END
};


static struct dependency dependencies[] = { {HARD, USERBUS}, {SOFT, BRIGHTNESS} };
static struct self_t self = {
    .name = "BusInterface",
    .idx = INTERFACE,
    .num_deps = SIZE(dependencies),
    .deps = dependencies,
    .standalone = 1
};

void set_interface_self(void) {
    SET_SELF();
}

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
    
    if (is_inited(BRIGHTNESS)) {
        set_timeout(0, 1, main_p[BRIGHTNESS].fd, 0);
        r = 0;
    } else {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Brightness module is not inited.");
        r = -EINVAL;
    }
    return r;
}
