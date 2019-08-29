#include "idler.h"

static int on_new_idle(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static void upower_timeout_callback(void);
static void inhibit_callback(void);

static sd_bus_slot *slot;
static char client[PATH_MAX + 1];

DECLARE_MSG(display_req, DISPLAY_REQ);

MODULE("DPMS");

static void init(void) {
    int r = idle_init(client, &slot, conf.dpms_timeout[state.ac_state], on_new_idle);
    if (r == 0) {
        M_SUB(UPOWER_UPD);
        M_SUB(INHIBIT_UPD);
        M_SUB(DPMS_TO_REQ);
    } else {
        WARN("Failed to init.\n");
        m_poisonpill(self());
    }
}

/* Works everywhere except wayland */
static bool check(void) {
    return !state.wl_display;
}

static bool evaluate(void) {
    return !conf.no_dpms && state.ac_state != -1;
}

static void receive(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
    case UPOWER_UPD:
        upower_timeout_callback();
        break;
    case INHIBIT_UPD:
        inhibit_callback();
        break;
    case DPMS_TO_REQ: {
        timeout_upd *up = (timeout_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            conf.dpms_timeout[up->state] = up->new;
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
    idle_client_destroy(client);
}

static int on_new_idle(sd_bus_message *m,  UNUSED void *userdata, UNUSED sd_bus_error *ret_error) {   
    int is_dpms;
    sd_bus_message_read(m, "b", &is_dpms);
    
    /* Unused in requests! */
    display_req.display.old = state.display_state;
    if (is_dpms) {
        display_req.display.new = DISPLAY_OFF;
    } else {
        display_req.display.new = ~DISPLAY_OFF;
    }
    M_PUB(&display_req);
    return 0;
}

static void upower_timeout_callback(void) {
    idle_set_timeout(client, conf.dpms_timeout[state.ac_state]);
}

/*
 * If we're getting inhibited, stop idle client.
 * Else, restart it.
 */
static void inhibit_callback(void) {
    if (!state.inhibited) {
        DEBUG("Being resumed.\n");
        idle_client_start(client, conf.dpms_timeout[state.ac_state]);
    } else {
        DEBUG("Being paused.\n");
        idle_client_stop(client);
    }
}
