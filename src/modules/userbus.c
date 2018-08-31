#include <bus.h>

static struct dependency dependencies[] = { {SUBMODULE, BUS} };
static struct self_t self = {
    .num_deps = SIZE(dependencies),
    .deps =  dependencies
};

MODULE(USERBUS);

/*
 * Open our bus and start lisetining on its fd
 */
static void init(void) {
    sd_bus **userbus = get_user_bus();
    int r = sd_bus_default_user(userbus);
    if (r < 0) {
        ERROR("Failed to connect to user bus: %s\n", strerror(-r));
    }
    // let main poll listen on bus events
    int bus_fd = sd_bus_get_fd(*userbus);
    INIT_MOD(bus_fd);
}

static int check(void) {
    return 0; /* Skeleton function needed for modules interface */
}

/*
 * Close bus
 */
static void destroy(void) {
    sd_bus **userbus = get_user_bus();
    if (*userbus) {
        *userbus = sd_bus_flush_close_unref(*userbus);
    }
}

/*
 * Callback for bus events
 */
static void callback(void) {
    bus_callback();
}
