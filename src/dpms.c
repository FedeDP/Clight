#include "../inc/dpms.h"
#include "../inc/bus.h"
#include "../inc/upower.h"

static void init(void);
static int check(void);
static void destroy(void);
static void set_dpms(void);
static void upower_callback(int old_state);

static struct dependency dependencies[] = { {SOFT, UPOWER}, {HARD, BUS} };
static struct self_t self = {
    .name = "Dpms",
    .idx = DPMS,
    .num_deps = SIZE(dependencies),
    .deps =  dependencies
};

void set_dpms_self(void) {
    modules[self.idx].self = &self;
    modules[self.idx].init = init;
    modules[self.idx].check = check;
    modules[self.idx].destroy = destroy;
    set_self_deps(&self);
}


static void init(void) {
    set_dpms();
    init_module(DONT_POLL, self.idx, NULL);
    if (!modules[self.idx].disabled) {
        add_upower_module_callback(upower_callback);
    }
}

/* Check module is not disabled, we're on X and proper configs are set. */
static int check(void) {
    return conf.no_dpms || !state.display || !state.xauthority;
}

static void destroy(void) {
    /* Skeleton function */
}

static void set_dpms(void) {
    struct bus_args args_get = {"org.clightd.backlight", "/org/clightd/backlight", "org.clightd.backlight", "setdpms_timeouts"};
    bus_call(NULL, NULL, &args_get, "ssiii", state.display, state.xauthority, 
             conf.dpms_timeouts[state.ac_state][STANDBY], conf.dpms_timeouts[state.ac_state][SUSPEND], conf.dpms_timeouts[state.ac_state][OFF]);
}

static void upower_callback(__attribute__((unused)) int old_state) {
    set_dpms();
}
