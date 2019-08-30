#include "idler.h"

enum CLIENTS_TYPE { DIMMER, DPMS, CLIENTS_SIZE };

static int on_new_idle(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static void upower_timeout_callback(enum CLIENTS_TYPE type, int *timeout);
static void inhibit_callback(void);

static sd_bus_slot *slot[CLIENTS_SIZE];
static char client[PATH_MAX + 1][CLIENTS_SIZE];

DECLARE_MSG(display_req, DISPLAY_REQ);

MODULE("IDLE");

static void init(void) {
    int r = 0;
    if (!conf.no_dimmer) {
        r += idle_init(client[DIMMER], &slot[DIMMER], conf.dimmer_timeout[state.ac_state], on_new_idle) == 0;
    }
    if (!conf.no_dpms) {
        r += idle_init(client[DPMS], &slot[DPMS], conf.dpms_timeout[state.ac_state], on_new_idle) == 0;
    }
    if (r) {
        M_SUB(UPOWER_UPD);
        M_SUB(INHIBIT_UPD);
        M_SUB(DIMMER_TO_REQ);
        M_SUB(DPMS_TO_REQ);
    } else {
        WARN("Failed to init.\n");
        m_poisonpill(self());
    }
}

static bool check(void) {
    return true;
}

static bool evaluate(void) {
    return (!conf.no_dimmer || !conf.no_dpms) && state.ac_state != -1;
}

static void receive(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
    case UPOWER_UPD:
        upower_timeout_callback(DIMMER, conf.dimmer_timeout);
        upower_timeout_callback(DPMS, conf.dpms_timeout);
        break;
    case INHIBIT_UPD:
        inhibit_callback();
        break;
    case DIMMER_TO_REQ: {
        timeout_upd *up = (timeout_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            conf.dimmer_timeout[up->state] = up->new;
            if (up->state == state.ac_state) {
                upower_timeout_callback(DIMMER, conf.dimmer_timeout);
            }
        }
        break;
    }
    case DPMS_TO_REQ: {
        timeout_upd *up = (timeout_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            conf.dpms_timeout[up->state] = up->new;
            if (up->state == state.ac_state) {
                upower_timeout_callback(DPMS, conf.dpms_timeout);
            }
        }
        break;
    }
    default:
        break;
    }
}

static void destroy(void) {
    if (slot[DIMMER]) {
        slot[DIMMER] = sd_bus_slot_unref(slot[DIMMER]);
    }
    if (slot[DPMS]) {
        slot[DPMS] = sd_bus_slot_unref(slot[DPMS]);
    }
    idle_client_destroy(client[DIMMER]);
    idle_client_destroy(client[DPMS]);
}

static int on_new_idle(sd_bus_message *m, UNUSED void *userdata, UNUSED sd_bus_error *ret_error) {
    int idle;
    
    sd_bus *b = sd_bus_message_get_bus(m);
    const bool is_dpms = sd_bus_get_current_slot(b) == slot[DPMS];
    
    /* Unused in requests! */
    display_req.display.old = state.display_state;
    sd_bus_message_read(m, "b", &idle);
    if (idle) {
        display_req.display.new = is_dpms ? DISPLAY_OFF : DISPLAY_DIMMED;
    } else if (!is_dpms) {
        /* 
         * We only manage a single request for display on. 
         * Avoid requesting for both idle clients; manage it only for dimmer 
         * as we may leave DIMMED state without leaving DPMS state, but not the contrary.
         */
        display_req.display.new = DISPLAY_ON;
    } else {
        return 0;
    }
    M_PUB(&display_req);
    return 0;
}

/* Reset dimmer timeout */
static void upower_timeout_callback(enum CLIENTS_TYPE type, int *timeout) {
    idle_set_timeout(client[type], timeout[state.ac_state]);
}

/*
 * If we're getting inhibited, stop idle client.
 * Else, restart it.
 */
static void inhibit_callback(void) {
    if (!state.inhibited) {
        DEBUG("Being resumed.\n");
        idle_client_start(client[DIMMER], conf.dimmer_timeout[state.ac_state]);
        idle_client_start(client[DPMS], conf.dpms_timeout[state.ac_state]);
    } else {
        DEBUG("Being paused.\n");
        idle_client_stop(client[DIMMER]);
        idle_client_stop(client[DPMS]);
    }
}
