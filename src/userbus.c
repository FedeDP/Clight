#include "../inc/userbus.h"
#include "../inc/bus.h"

static void init(void);
static int check(void);
static void destroy(void);
static void callback(void);

static sd_bus *userbus;
static struct dependency dependencies[] = { {HARD, BUS} };
static struct self_t self = {
    .name = "UserBus",
    .idx = USERBUS,
    .num_deps = SIZE(dependencies),
    .deps =  dependencies
};

void set_userbus_self(void) {
    SET_SELF();
}

/*
 * Open our bus and start lisetining on its fd
 */
static void init(void) {
    int r = sd_bus_default_user(&userbus);
    if (r < 0) {
        ERROR("Failed to connect to user bus: %s\n", strerror(-r));
    }
    // let main poll listen on bus events
    int bus_fd = sd_bus_get_fd(userbus);
    INIT_MOD(bus_fd);
}

static int check(void) {
    return 0; /* Skeleton function needed for modules interface */
}

/*
 * Close bus
 */
static void destroy(void) {
    if (userbus) {
        userbus = sd_bus_flush_close_unref(userbus);
    }
}

/*
 * Callback for bus events
 */
static void callback(void) {
    bus_callback();
}

sd_bus *get_user_bus(void) {
    return userbus;
}
