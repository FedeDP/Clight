#include "bus.h"

static int upower_check(void);
static int upower_init(void);
static int on_upower_change(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static void publish_upower(int new, message_t *up);
static void publish_lid(bool new, message_t *up);
static void publish_inh(bool new, message_t *up);

static sd_bus_slot *slot;

DECLARE_MSG(upower_msg, UPOWER_UPD);
DECLARE_MSG(upower_req, UPOWER_REQ);
DECLARE_MSG(lid_msg, LID_UPD);
DECLARE_MSG(lid_req, LID_REQ);
DECLARE_MSG(inh_req, INHIBIT_REQ);

MODULE("UPOWER");

static void init(void) {
    if (upower_check() != 0 || upower_init() != 0) {
        /* Upower not available. Let's assume ON_AC! */
        state.ac_state = ON_AC;
        INFO("Failed to retrieve AC state; fallback to connected.\n");
        m_poisonpill(self());
    } else {
        INFO("Initial AC state: %s.\n", state.ac_state == ON_AC ? "connected" : "disconnected");
        M_SUB(UPOWER_REQ);
        M_SUB(LID_REQ);
    }
}

static bool check(void) {
    return true;
}

static bool evaluate(void) {
    return true;
}

static void receive(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
    case UPOWER_REQ: {
        upower_upd *up = (upower_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            INFO("AC cable %s.\n", up->new == ON_AC ? "connected" : "disconnected");
            // publish upower before storing new ac state as state.ac_state is sent as "old" parameter
            publish_upower(up->new, &upower_msg);
            state.ac_state = up->new;
        }
        break;
    }
    case LID_REQ: {
        lid_upd *up = (lid_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            if (up->new != DOCKED) {
                INFO("Lid %s.\n", up->new == CLOSED ? "closed" : "opened");
            } else {
                INFO("Laptop docked.\n");
            }
            // publish lid before storing new lid state as state.lid_closed is sent as "old" parameter
            publish_lid(up->new, &lid_msg);
            state.lid_state = up->new;
        }
        break;
    }
    default:
        break;
    }
}

static void destroy(void) {
    /* Destroy this match slot */
    if (slot) {
        slot = sd_bus_slot_unref(slot);
    }
}

static int upower_check(void) {
    bool is_laptop = false;
    SYSBUS_ARG(lid_pres_args, "org.freedesktop.UPower",  "/org/freedesktop/UPower", "org.freedesktop.UPower", "LidIsPresent");
    int r = get_property(&lid_pres_args, "b", &is_laptop, sizeof(is_laptop));
    if (!r) {
        if (is_laptop) {
            /* check initial AC state and lid state */
            SYSBUS_ARG(batt_args, "org.freedesktop.UPower",  "/org/freedesktop/UPower", "org.freedesktop.UPower", "OnBattery");
            SYSBUS_ARG(lid_close_args, "org.freedesktop.UPower",  "/org/freedesktop/UPower", "org.freedesktop.UPower", "LidIsClosed");
            
            r = get_property(&batt_args, "b", &state.ac_state, sizeof(state.ac_state));
            r += get_property(&lid_close_args, "b", &state.lid_state, sizeof(state.lid_state));
        } else {
            INFO("Not a laptop device. Killing UPower module.\n");
            r = -1;
        }
    } else {
        INFO("UPower not present. Killing UPower module.\n");
    }
    return -(r < 0);
}

static int upower_init(void) {
    SYSBUS_ARG(args, "org.freedesktop.UPower", "/org/freedesktop/UPower", "org.freedesktop.DBus.Properties", "PropertiesChanged");
    return add_match(&args, &slot, on_upower_change);
}

/*
 * Callback on upower changes: recheck "OnBattery" and "LidIsClosed" boolean values
 */
static int on_upower_change(UNUSED sd_bus_message *m, UNUSED void *userdata, UNUSED sd_bus_error *ret_error) {
    SYSBUS_ARG(batt_args, "org.freedesktop.UPower",  "/org/freedesktop/UPower", "org.freedesktop.UPower", "OnBattery");
    SYSBUS_ARG(lid_close_args, "org.freedesktop.UPower",  "/org/freedesktop/UPower", "org.freedesktop.UPower", "LidIsClosed");
    /*
     * Store last ac_state in old struct to be matched against new one
     * as we cannot be sure that a OnBattery changed signal has been really sent:
     * our match will receive these signals:
     * .DaemonVersion                      property  s         "0.99.5"     emits-change
     * .LidIsClosed                        property  b         true         emits-change
     * .LidIsPresent                       property  b         true         emits-change
     * .OnBattery                          property  b         false        emits-change
     */
    int ac_state;
    int r = get_property(&batt_args, "b", &ac_state, sizeof(ac_state));
    if (!r && state.ac_state != ac_state) {
        publish_upower(ac_state, &upower_req);
    }
    
    enum lid_states lid_state;
    r = get_property(&lid_close_args, "b", &lid_state, sizeof(lid_state));
    if (!r && !!state.lid_state != lid_state) {
        if (conf.inhibit_docked) {
            SYSBUS_ARG(docked_args, "org.freedesktop.login1",  "/org/freedesktop/login1", "org.freedesktop.login1.Manager", "Docked");
            
            bool docked;
            int r = get_property(&docked_args, "b", &docked, sizeof(docked));
            if (!r && docked != (state.lid_state == DOCKED)) {
                lid_state += docked;
                publish_inh(docked, &inh_req);
            }
        }
        publish_lid(lid_state, &lid_req);
    }
    return 0;
}

static void publish_upower(int new, message_t *up) {
    up->upower.old = state.ac_state;
    up->upower.new = new;
    M_PUB(up);
}

static void publish_lid(bool new, message_t *up) {
    up->lid.old = state.lid_state;
    up->lid.new = new;
    M_PUB(up);
}

static void publish_inh(bool new, message_t *up) {
    up->inhibit.old = state.inhibited;
    up->inhibit.new = new;
    up->inhibit.reason = strdup("Docked laptop");
    M_PUB(up);
}
