#include <bus.h>
#include <interface.h>

static int inhibit_init(void);
static int inhibit_check(void);
static int on_inhibit_change(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

static sd_bus_slot *slot;
static inhibit_upd inh_msg = { INHIBIT_UPDATE };

const char *inh_topic = "PmState";

MODULE("INHIBIT");

static void init(void) {
    if (inhibit_init() != 0) {
        WARN("Failed to init.\n");
        m_poisonpill(self());
    }
}

static bool check(void) {
    return true;
}

static bool evaluate() {
    /* Start as soon as PowerManagement.Inhibit becomes available */
    return conf.no_inhibit == 0 && inhibit_check() == 0;
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

static int inhibit_check(void) {
    /* check initial inhibit state (if interface is found) */
    USERBUS_ARG(args, "org.freedesktop.PowerManagement.Inhibit", "/org/freedesktop/PowerManagement/Inhibit", "org.freedesktop.PowerManagement.Inhibit", "HasInhibit");
    int r = call(&state.pm_inhibited, "b", &args, NULL);
    return -(r < 0);
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
            inh_msg.old = old_inhibited;
            inh_msg.new = state.pm_inhibited;
            emit_prop(inh_topic, self(), &inh_msg, sizeof(inhibit_upd));
        }
    }
    return 0;
}
