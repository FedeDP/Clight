#include <bus.h>

static int inhibit_init(void);
static int inhibit_check(void);
static int on_inhibit_change(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static void publish_inhibit(int old, int new, inhibit_upd *up);

static sd_bus_slot *slot;
static inhibit_upd inh_msg = { INHIBIT_UPD };
static inhibit_upd inh_req = { INHIBIT_REQ };

MODULE("INHIBIT");

static void init(void) {
    if (inhibit_init() != 0) {
        WARN("Failed to init.\n");
        m_poisonpill(self());
    } else {
        M_SUB(INHIBIT_REQ);
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
    if (msg->is_pubsub && msg->ps_msg->type == USER) {
        MSG_TYPE();
        switch (type) {
        case INHIBIT_REQ: {
            inhibit_upd *up = (inhibit_upd *)msg->ps_msg->message;
            state.pm_inhibited = up->new;
            if (up->new & PM_FORCED_ON || up->old & PM_FORCED_ON) {
                INFO("PowerManagement inhibition %s by user request.\n", up->new & PM_FORCED_ON ? "forced" : "released");
            } else {
                INFO("PowerManagement inhibition %s by freedesktop.PowerManagement.\n", state.pm_inhibited ? "enabled" : "disabled");
            }
            publish_inhibit(up->old, up->new, &inh_msg);
            }
            break;
        default:
            break;
        }
    }
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
static int on_inhibit_change(UNUSED sd_bus_message *m, UNUSED void *userdata, UNUSED sd_bus_error *ret_error) {
    USERBUS_ARG(args, "org.freedesktop.PowerManagement.Inhibit", "/org/freedesktop/PowerManagement/Inhibit", "org.freedesktop.PowerManagement.Inhibit", "HasInhibit");
    int inhibited;
    int r = call(&inhibited, "b", &args, NULL);
    if (!r) {
        int old_inhibited = state.pm_inhibited;
        if (inhibited) {
            inhibited = old_inhibited | PM_ON;
        } else {
            inhibited = old_inhibited & ~PM_ON;
        }
        if (old_inhibited != inhibited) {
            publish_inhibit(old_inhibited, inhibited, &inh_req);
        }
    }
    return 0;
}

static void publish_inhibit(int old, int new, inhibit_upd *up) {
    up->old = old;
    up->new = new;
    M_PUB(up);
}
