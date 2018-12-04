#include <bus.h>

#define DPMS_DISABLED -1

static void set_dpms(int dpms_state);
static void set_dpms_timeouts(void);
static void upower_callback(const void *ptr);
static void interface_timeout_callback(const void *ptr);

static struct dependency dependencies[] = { 
    {SOFT, UPOWER},     // Are we on AC or on BATT?
    {HARD, XORG},       // This module is xorg only
    {HARD, CLIGHTD}     // We need clightd
};
static struct self_t self = {
    .num_deps = SIZE(dependencies),
    .deps =  dependencies,
    .functional_module = 1
};

MODULE(DPMS);

static void init(void) {
    struct bus_cb upower_cb = { UPOWER, upower_callback };
    struct bus_cb interface_to_cb = { INTERFACE, interface_timeout_callback, "dpms_timeout" };

    set_dpms_timeouts();
    INIT_MOD(DONT_POLL, &upower_cb, &interface_to_cb);
}

/* Check module is not disabled, we're on X and proper configs are set. */
static int check(void) {
    return 0;
}

static void callback(void) {
    /* Skeleton interface */
}

static void destroy(void) {
    /* Skeleton function */
}

/* 
 * Set correct timeouts or disable dpms 
 * if any timeout for current AC state is <= 0.
 */
void set_dpms_timeouts(void) {
    int need_disable = 0;
    /* Init need_disable array */
    for (int i = 0; i < SIZE_DPMS && !need_disable; i++) {
        if (conf.dpms_timeouts[state.ac_state][i] <= 0) {
            need_disable = 1;
        }
    }

    if (!need_disable) {
        SYSBUS_ARG(args, CLIGHTD_SERVICE, "/org/clightd/clightd/Dpms", "org.clightd.clightd.Dpms", "SetTimeouts");
        call(NULL, NULL, &args, "ssiii", state.display, state.xauthority, 
             conf.dpms_timeouts[state.ac_state][STANDBY], conf.dpms_timeouts[state.ac_state][SUSPEND], conf.dpms_timeouts[state.ac_state][OFF]);
        INFO("Setted DPMS timeouts: Standby -> %ds, Suspend -> %ds, Off -> %ds.\n", 
             conf.dpms_timeouts[state.ac_state][STANDBY], 
             conf.dpms_timeouts[state.ac_state][SUSPEND], 
             conf.dpms_timeouts[state.ac_state][OFF]);
    } else {
        INFO("Disabling DPMS as a timeout <= 0 has been found.\n");
        set_dpms(DPMS_DISABLED);
    }
}

static void set_dpms(int dpms_state) {
    SYSBUS_ARG(args, CLIGHTD_SERVICE, "/org/clightd/clightd/Dpms", "org.clightd.clightd.Dpms", "Set");
    call(NULL, NULL, &args, "ssi", state.display, state.xauthority, dpms_state);
}

static void upower_callback(const void *ptr) {
    int old_ac_state = *(int *)ptr;
    /* Force check that we received an ac_state changed event for real */
    if (old_ac_state != state.ac_state) {
        set_dpms_timeouts();
    }
}

static void interface_timeout_callback(const void *ptr) {
    set_dpms_timeouts();
}
