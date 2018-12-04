#include <bus.h>

static int inhibit_check(void);
static int inhibit_init(void);
static int on_inhibit_change(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

static sd_bus_slot *slot;
static struct dependency dependencies[] = {
    {HARD, USERBUS}     // It needs a connection to user bus to be opened
};
static struct self_t self = {
    .num_deps = SIZE(dependencies),
    .deps = dependencies
};

MODULE(INHIBIT);

static void init(void) {
    int r = inhibit_init();
    /* In case of errors, inhibit_init returns -1 -> disable inhibit. */
    INIT_MOD(r == 0 ? DONT_POLL : DONT_POLL_W_ERR);
}

static int check(void) {
    inhibit_check();
    /*
     * Init INHIBIT module even if initial polling fails,
     * as interface may be later added.
     * We are allowed to add a match on non-existent bus interface.
     */
    return 0;
}

static void callback(void) {
    // Skeleton interface
}

static void destroy(void) {
    /* Destroy this match slot */
    if (slot) {
        slot = sd_bus_slot_unref(slot);
    }
}

static int inhibit_check(void) {
    /* check initial inhibit state (if interface is found) */
    USERBUS_ARG(args, "org.freedesktop.PowerManagement.Inhibit", "/org/freedesktop/PowerManagement/Inhibit", "org.freedesktop.PowerManagement.Inhibit", "HasInhibit");
    int r = call(&state.pm_inhibited, "b", &args, NULL);
    if (r < 0) {
        WARN("PowerManagement inhibition appears to be unsupported.\n");
    }
    return -(r < 0);
}

static int inhibit_init(void) {
    USERBUS_ARG(args, "org.freedesktop.PowerManagement", "/org/freedesktop/PowerManagement/Inhibit", "org.freedesktop.PowerManagement.Inhibit", "HasInhibitChanged");
    return add_match(&args, &slot, on_inhibit_change);
}

/*
 * Callback on inhibit state changed: recheck new HasInhibit value
 */
static int on_inhibit_change(__attribute__((unused)) sd_bus_message *m, void *userdata, __attribute__((unused)) sd_bus_error *ret_error) {
    if (state.pm_inhibited != PM_FORCED_ON) {
        FILL_MATCH_DATA(state.pm_inhibited);

        USERBUS_ARG(args, "org.freedesktop.PowerManagement.Inhibit", "/org/freedesktop/PowerManagement/Inhibit", "org.freedesktop.PowerManagement.Inhibit", "HasInhibit");
        int inhibited;
        int r = call(&inhibited, "b", &args, NULL);

        if (!r && state.pm_inhibited != inhibited) {
            state.pm_inhibited = inhibited;
            INFO("PowerManagement inhibition %s.\n", state.pm_inhibited ? "enabled" : "disabled");
        }
    } else {
        DEBUG("Pm Inhibition is currently forced ON by bus API.\n");
    }
    return 0;
}
