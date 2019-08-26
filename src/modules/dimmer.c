#include "idler.h"

static int on_new_idle(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static void dim_backlight(const double pct);
static void restore_backlight(const double pct);
static void upower_timeout_callback(void);
static void inhibit_callback(void);
static void publish_bl_req(const double pct, const bool smooth, const double step, const int to);

static sd_bus_slot *slot;
static char client[PATH_MAX + 1];

DECLARE_MSG(display_msg, DISPLAY_UPD);
DECLARE_MSG(bl_req, BL_REQ);

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

static void receive(const msg_t *const msg, const void* userdata) {
    if (msg->is_pubsub && msg->ps_msg->type == USER) {
        switch (MSG_TYPE()) {
            case UPOWER_UPD:
                upower_timeout_callback();
                break;
            case DIMMER_TO_REQ: {
                timeout_upd *up = (timeout_upd *)MSG_DATA();
                /* Validate */
                if (up->state >= ON_AC && up->state < SIZE_AC) {
                    conf.dimmer_timeout[up->state] = up->new;
                    if (up->state == state.ac_state) {
                        upower_timeout_callback();
                    }
                } else {
                    WARN("Failed to validate timeout request.\n");
                }
                }
                break;
            case INHIBIT_UPD:
                inhibit_callback();
                break;
            default:
                break;
        }
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
    static double old_pct = -1.0;
    int dimmed;
    
    display_msg.display.old = state.display_state;
    
    sd_bus_message_read(m, "b", &dimmed);
    if (dimmed) {
        state.display_state |= DISPLAY_DIMMED;
        DEBUG("Entering dimmed state...\n");
        old_pct = state.current_bl_pct;
        dim_backlight(conf.dimmer_pct);
    } else if (old_pct >= 0.0) {
        state.display_state &= ~DISPLAY_DIMMED;
        DEBUG("Leaving dimmed state...\n");
        restore_backlight(old_pct);
    }
    
    display_msg.display.new = state.display_state;
    M_PUB(&display_msg);
    return 0;
}

static void dim_backlight(const double pct) {
    /* Don't touch backlight if a lower level is already set */
    if (pct >= state.current_bl_pct) {
        DEBUG("A lower than dimmer_pct backlight level is already set. Avoid changing it.\n");
    } else {
        publish_bl_req(pct, !conf.no_smooth_dimmer[ENTER], conf.dimmer_trans_step[ENTER], conf.dimmer_trans_timeout[ENTER]);
    }
}

/* restore previous backlight level */
static void restore_backlight(const double pct) {
    publish_bl_req(pct, !conf.no_smooth_dimmer[EXIT], conf.dimmer_trans_step[EXIT], conf.dimmer_trans_timeout[EXIT]);
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
        idle_client_start(client);
    } else {
        DEBUG("Being paused.\n");
        idle_client_stop(client);
    }
}

static void publish_bl_req(const double pct, const bool smooth, const double step, const int to) {
    bl_req.bl.new = pct;
    bl_req.bl.smooth = smooth;
    bl_req.bl.step = step;
    bl_req.bl.timeout = to;
    M_PUB(&bl_req);
}
