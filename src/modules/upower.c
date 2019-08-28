#include "bus.h"

static int upower_check(void);
static int upower_init(void);
static int on_upower_change(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static void publish_upower(int new, message_t *up);

static sd_bus_slot *slot;

DECLARE_MSG(upower_msg, UPOWER_UPD);
DECLARE_MSG(upower_req, UPOWER_REQ);

MODULE("UPOWER");

static void init(void) {
    if (upower_init() != 0) {
        WARN("Failed to init.\n");
        m_poisonpill(self());
    } else {
        M_SUB(UPOWER_REQ);
    }
}

static bool check(void) {
    return true;
}

static bool evaluate(void) {
    /* Start as soon as upower becomes available */
    return upower_check() == 0;
}

static void receive(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
    case UPOWER_REQ: {
        upower_upd *up = (upower_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            INFO("AC cable %s.\n", up->new ? "connected" : "disconnected");
            // publish upower before storing new ac state as state.ac_state is sent as "old" parameter
            publish_upower(up->new, &upower_msg);
            state.ac_state = up->new;
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
    /* check initial AC state */
    SYSBUS_ARG(args, "org.freedesktop.UPower",  "/org/freedesktop/UPower", "org.freedesktop.UPower", "OnBattery");
    int r = get_property(&args, "b", &state.ac_state, sizeof(state.ac_state));
    if (r < 0) {
        if (state.ac_state == -1) {
            /* Upower not available, for now. Let's assume ON_AC! */
            state.ac_state = ON_AC;
            INFO("Failed to retrieve AC state; fallback to connected.\n");
        }
    } else {
        INFO("Initial AC state: %s.\n", state.ac_state == ON_AC ? "connected" : "disconnected");
    }
    return -(r < 0);
}

static int upower_init(void) {
    SYSBUS_ARG(args, "org.freedesktop.UPower", "/org/freedesktop/UPower", "org.freedesktop.DBus.Properties", "PropertiesChanged");
    return add_match(&args, &slot, on_upower_change);
}

/*
 * Callback on upower changes: recheck on_battery boolean value
 */
static int on_upower_change(UNUSED sd_bus_message *m, UNUSED void *userdata, UNUSED sd_bus_error *ret_error) {
    SYSBUS_ARG(args, "org.freedesktop.UPower",  "/org/freedesktop/UPower", "org.freedesktop.UPower", "OnBattery");

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
    int r = get_property(&args, "b", &ac_state, sizeof(ac_state));
    if (!r && state.ac_state != ac_state) {
        publish_upower(ac_state, &upower_req);
    }
    return 0;
}

static void publish_upower(int new, message_t *up) {
    up->upower.old = state.ac_state;
    up->upower.new = new;
    M_PUB(up);
}
