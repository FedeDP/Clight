#include "../inc/dpms.h"
#include "../inc/bus.h"
#include "../inc/upower.h"

static void init(void);
static int check(void);
static void callback(void);
static void destroy(void);
static void set_dpms(void);
static void upower_callback(void);

static struct dependency dependencies[] = { {SOFT, UPOWER}, {HARD, BUS} };
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
    set_dpms();
    init_module(DONT_POLL, self.idx);
    if (!modules[self.idx].disabled) {
        struct bus_cb upower_cb = { UPOWER, upower_callback };
        add_mod_callback(upower_cb);
    }
}

/* Check module is not disabled, we're on X and proper configs are set. */
static int check(void) {
    return conf.single_capture_mode ||
            conf.no_dpms ||
           !state.display || 
           !state.xauthority;
}

static void callback(void) {
    // Skeleton interface
}

static void destroy(void) {
    /* Skeleton function */
}

static void set_dpms(void) {
    struct bus_args args_get = {"org.clightd.backlight", "/org/clightd/backlight", "org.clightd.backlight", "setdpms_timeouts"};
    bus_call(NULL, NULL, &args_get, "ssiii", state.display, state.xauthority, 
             conf.dpms_timeouts[state.ac_state][STANDBY], conf.dpms_timeouts[state.ac_state][SUSPEND], conf.dpms_timeouts[state.ac_state][OFF]);
}

static void upower_callback(void) {
    /* Force check that we received an ac_state changed event for real */
    if (state.old_ac_state != state.ac_state) {
        set_dpms();
    }
}
