#include "../inc/inhibit.h"
#include "../inc/bus.h"

static void init(void);
static int check(void);
static void callback(void);
static void destroy(void);
static int inhibit_init(void);
static int on_inhibit_change(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

static sd_bus_slot *slot;
static struct dependency dependencies[] = { {HARD, USERBUS}, {HARD, XORG} };
static struct self_t self = {
    .name = "Inhibit",
    .idx = INHIBIT,
    .num_deps = SIZE(dependencies),
    .deps =  dependencies
};

void set_inhibit_self(void) {
    SET_SELF();
}

static void init(void) {
    int r = inhibit_init();
    /* In case of errors, upower_init returns -1 -> disable inhibit. */
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

/* It needs userbus */
static int inhibit_init(void) {
    struct bus_args args = {"org.freedesktop.PowerManagement", "/org/freedesktop/PowerManagement/Inhibit", "org.freedesktop.PowerManagement.Inhibit", "HasInhibitChanged", USER};
    int r = add_match(&args, &slot, on_inhibit_change);
    if (r < 0) {
        WARN("PowerManagement inhibition appears to be unsupported.\n");
        return -1;   // disable this module
    }
    /* check initial inhibit state */
    return on_inhibit_change(NULL, NULL, NULL);
}

/* 
 * Callback on inhibit state changed: recheck new HasInhibit value
 */
static int on_inhibit_change(__attribute__((unused)) sd_bus_message *m, void *userdata, __attribute__((unused)) sd_bus_error *ret_error) {
    if (userdata) {
        *(int *)userdata = self.idx;
    }
    
    struct bus_args args = {"org.freedesktop.PowerManagement.Inhibit", "/org/freedesktop/PowerManagement/Inhibit", "org.freedesktop.PowerManagement.Inhibit", "HasInhibit", USER};
    call(&state.pm_inhibited, "b", &args, "");
    
    if (userdata) {
        INFO("PowerManagement inhibition %s.\n", state.pm_inhibited ? "enabled" : "disabled");
    }
    return 0;
}

