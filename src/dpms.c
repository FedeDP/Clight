#include "../inc/dpms.h"
#include "../inc/bus.h"
#include "../inc/upower.h"

#define DPMS_DISABLED -1

static void init(void);
static int check(void);
static void callback(void);
static void destroy(void);
static void set_dpms_timeouts(void);
static void set_dpms(int dpms_state);
static void upower_callback(const void *ptr);

static struct dependency dependencies[] = { {SOFT, UPOWER}, {HARD, BUS}, {HARD, XORG} };
static struct self_t self = {
    .name = "Dpms",
    .idx = DPMS,
    .num_deps = SIZE(dependencies),
    .deps =  dependencies
};

void set_dpms_self(void) {
    SET_SELF();
}

static void init(void) {
    struct bus_cb upower_cb = { UPOWER, upower_callback };
    
    /* pass a non-void ptr to avoid logging in inhibit_callback */
    set_dpms_timeouts();
    INIT_MOD(DONT_POLL, &upower_cb);
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
static void set_dpms_timeouts(void) {
    int need_disable = 0;
    for (int i = 0; i < SIZE_DPMS && !need_disable; i++) {
        if (conf.dpms_timeouts[state.ac_state][i] <= 0) {
            need_disable = 1;
        }
    }
    
    if (!need_disable) {
        struct bus_args args_get = {"org.clightd.backlight", "/org/clightd/backlight", "org.clightd.backlight", "setdpms_timeouts"};
        call(NULL, NULL, &args_get, "ssiii", state.display, state.xauthority, 
             conf.dpms_timeouts[state.ac_state][STANDBY], conf.dpms_timeouts[state.ac_state][SUSPEND], conf.dpms_timeouts[state.ac_state][OFF]);
    } else {
        INFO("Disabling DPMS as a timeout <= 0 has been found.\n");
        set_dpms(DPMS_DISABLED);
    }
}

static void set_dpms(int dpms_state) {
    struct bus_args args_get = {"org.clightd.backlight", "/org/clightd/backlight", "org.clightd.backlight", "setdpms"};
    call(NULL, NULL, &args_get, "ssi", state.display, state.xauthority, dpms_state);
}

static void upower_callback(const void *ptr) {
    int old_ac_state = *(int *)ptr;
    /* Force check that we received an ac_state changed event for real */
    if (old_ac_state != state.ac_state) {
        set_dpms_timeouts();
    }
}
