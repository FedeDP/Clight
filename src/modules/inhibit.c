#include <bus.h>
#include <interface.h>

static int inhibit_init(void);
static int on_inhibit_change(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

static sd_bus_slot *slot;
static struct dependency dependencies[] = {
    {HARD, BUS}     // It needs a connection to user bus to be opened
};
static struct self_t self = {
    .num_deps = SIZE(dependencies),
    .deps = dependencies
};

MODULE("INHIBIT");

static void init(void) {
    int r = inhibit_init();
    /* In case of errors, inhibit_init returns -1 -> disable inhibit. */
    INIT_MOD(r == 0 ? DONT_POLL : DONT_POLL_W_ERR);
}

static bool check(void) {
    return true;
}

static bool evaluate() {
    return true;
}

static void receive(const msg_t *const msg, const void* userdata) {
    
}

static void destroy(void) {
    /* Destroy this match slot */
    if (slot) {
        slot = sd_bus_slot_unref(slot);
    }
}

static int inhibit_init(void) {
    USERBUS_ARG(args, "org.freedesktop.PowerManagement", "/org/freedesktop/PowerManagement/Inhibit", "org.freedesktop.PowerManagement.Inhibit", "HasInhibitChanged");
    return add_match(&args, &slot, on_inhibit_change);
}

/*
 * Callback on inhibit state changed: recheck new HasInhibit value
 */
static int on_inhibit_change(__attribute__((unused)) sd_bus_message *m, void *userdata, __attribute__((unused)) sd_bus_error *ret_error) {
    USERBUS_ARG(args, "org.freedesktop.PowerManagement.Inhibit", "/org/freedesktop/PowerManagement/Inhibit", "org.freedesktop.PowerManagement.Inhibit", "HasInhibit");
    int inhibited;
    int r = call(&inhibited, "b", &args, NULL);
    if (!r) {
        int old_inhibited = state.pm_inhibited;
        if (inhibited) {
            state.pm_inhibited |= PM_ON;
            INFO("PowerManagement inhibition enabled by freedesktop.PowerManagement.\n");
        } else {
            state.pm_inhibited &= ~PM_ON;
            INFO("PowerManagement inhibition disabled by freedesktop.PowerManagement.\n");
        }
        if (old_inhibited != state.pm_inhibited) {
            FILL_MATCH_NONE();
            emit_prop("PmState");
        }
    }
    return 0;
}
