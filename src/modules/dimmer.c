#include "idler.h"

static int on_new_idle(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static void upower_timeout_callback(void);
static void inhibit_callback(void);

static sd_bus_slot *slot;
static char client[PATH_MAX + 1];

DECLARE_MSG(display_req, DISPLAY_REQ);

MODULE("DIMMER");

static void init(void) {
    int r = idle_init(client, &slot, conf.dimmer_timeout[state.ac_state], on_new_idle);
    if (r == 0) {
        M_SUB(UPOWER_UPD);
        M_SUB(INHIBIT_UPD);
        M_SUB(DIMMER_TO_REQ);
    } else {
        WARN("Failed to init.\n");
        m_poisonpill(self());
    }
}

static bool check(void) {
    return true;
}

static bool evaluate(void) {
    return !conf.no_dimmer && state.ac_state != -1;
}

static void receive(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
    case UPOWER_UPD:
        upower_timeout_callback();
        break;
    case INHIBIT_UPD:
        inhibit_callback();
        break;
    case DIMMER_TO_REQ: {
        timeout_upd *up = (timeout_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            conf.dimmer_timeout[up->state] = up->new;
            if (up->state == state.ac_state) {
                upower_timeout_callback();
            }
        }
        break;
    }
    default:
        break;
    }
}

static void destroy(void) {
    if (slot) {
        slot = sd_bus_slot_unref(slot);
    }
    idle_client_stop(client);
    idle_client_destroy(client);
}

static int on_new_idle(sd_bus_message *m, UNUSED void *userdata, UNUSED sd_bus_error *ret_error) {
    int dimmed;
    
    /* Unused in requests! */
    display_req.display.old = state.display_state;
    sd_bus_message_read(m, "b", &dimmed);
    if (dimmed) {
        display_req.display.new = DISPLAY_DIMMED;
    } else {
        display_req.display.new = ~DISPLAY_DIMMED;
    }
    M_PUB(&display_req);
    return 0;
}

/* Reset dimmer timeout */
static void upower_timeout_callback(void) {
    idle_set_timeout(client, conf.dimmer_timeout[state.ac_state]);
}

/*
 * If we're getting inhibited, stop idle client.
 * Else, restart it.
 */
static void inhibit_callback(void) {
    if (!state.inhibited) {
        DEBUG("Being resumed.\n");
        idle_client_start(client, conf.dimmer_timeout[state.ac_state]);
    } else {
        DEBUG("Being paused.\n");
        idle_client_stop(client);
    }
}
