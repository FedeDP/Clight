#include "../inc/network.h"

static void init(void);
static int check(void);
static void callback(void);
static void destroy(void);
static int network_init(void);
static int on_network_change(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

static sd_bus_slot *slot;
static struct dependency dependencies[] = { {HARD, BUS} };
static struct self_t self = {
    .name = "Network",
    .idx = NETWORK,
    .num_deps = SIZE(dependencies),
    .deps =  dependencies
};

void set_network_self(void) {
    SET_SELF();
}

static void init(void) {
    int r = network_init();
    /* In case of errors, upower_init returns -1 -> disable upower. */
    INIT_MOD(r == 0 ? DONT_POLL : DONT_POLL_W_ERR);
}

static int check(void) {
    return 0; /* Skeleton function needed for modules interface */
}

static void callback(void) {
    // Skeleton interface
}

static void destroy(void) {
    /* Destroy this match slot */
    if (slot) {
        sd_bus_slot_unref(slot);
    }
}

static int network_init(void) {
    /* check initial AC state */
    struct bus_args network_args = {"org.freedesktop.NetworkManager",  "/org/freedesktop/NetworkManager", "org.freedesktop.NetworkManager", "State"};
    int r = get_property(&network_args, "u", &state.nmstate);
    if (r < 0) {
        WARN("NetworkManager appears to be unsupported.\n");
        return -1;   // disable this module
    }
    struct bus_args args = {"org.freedesktop.NetworkManager",  "/org/freedesktop/NetworkManager", "org.freedesktop.NetworkManager", "StateChanged"};
    r = add_match(&args, &slot, on_network_change);
    return -(r < 0);
}

/* 
 * Callback on network changes: recheck networkmanager state value
 */
static int on_network_change(__attribute__((unused)) sd_bus_message *m, void *userdata, __attribute__((unused)) sd_bus_error *ret_error) {
    struct bus_match_data *data = (struct bus_match_data *) userdata;
    data->bus_mod_idx = self.idx;
    /* Fill data->ptr with old networkmanager state */
    data->ptr = malloc(sizeof(int));
    *(int *)(data->ptr) = state.nmstate;
    
    struct bus_args network_args = {"org.freedesktop.NetworkManager",  "/org/freedesktop/NetworkManager", "org.freedesktop.NetworkManager", "State"};
    get_property(&network_args, "u", &state.nmstate);
    if (*(int *)(data->ptr) != state.nmstate) {
        INFO("New network state: %u.\n", state.nmstate);
    }
    return 0;
}

/*
 * Returns network enabled boolean for a given NMState state.
 * Note that in case this module is disabled,
 * we assume network is always on.
 */
int network_enabled(const enum NMState nmstate) {
    return nmstate == NM_STATE_CONNECTED_GLOBAL || is_disabled(self.idx);
}
