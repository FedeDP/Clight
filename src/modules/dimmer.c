#include "idler.h"
#include "utils.h"

static void receive_waiting_acstate(const msg_t *msg, UNUSED const void *userdata);
static void receive_paused(const msg_t *const msg, UNUSED const void* userdata);
static int on_new_idle(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static void timeout_callback(void);
static void pause_dimmer(const bool pause, enum mod_pause reason);

static sd_bus_slot *slot;
static char client[PATH_MAX + 1];
static const sd_bus_vtable conf_dimmer_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_WRITABLE_PROPERTY("NoSmoothEnter", "b", NULL, NULL, offsetof(dimmer_conf_t, smooth[ENTER].no_smooth), 0),
    SD_BUS_WRITABLE_PROPERTY("NoSmoothExit", "b", NULL, NULL, offsetof(dimmer_conf_t, smooth[EXIT].no_smooth), 0),
    SD_BUS_WRITABLE_PROPERTY("DimmedPct", "d", NULL, NULL, offsetof(dimmer_conf_t, dimmed_pct), 0),
    SD_BUS_WRITABLE_PROPERTY("TransStepEnter", "d", NULL, NULL, offsetof(dimmer_conf_t, smooth[ENTER].trans_step), 0),
    SD_BUS_WRITABLE_PROPERTY("TransStepExit", "d", NULL, NULL, offsetof(dimmer_conf_t, smooth[EXIT].trans_step), 0),
    SD_BUS_WRITABLE_PROPERTY("TransDurationEnter", "i", NULL, NULL, offsetof(dimmer_conf_t, smooth[ENTER].trans_timeout), 0),
    SD_BUS_WRITABLE_PROPERTY("TransDurationExit", "i", NULL, NULL, offsetof(dimmer_conf_t, smooth[EXIT].trans_timeout), 0),
    SD_BUS_WRITABLE_PROPERTY("TransFixedEnter", "i", NULL, NULL, offsetof(dimmer_conf_t, smooth[ENTER].trans_fixed), 0),
    SD_BUS_WRITABLE_PROPERTY("TransFixedExit", "i", NULL, NULL, offsetof(dimmer_conf_t, smooth[EXIT].trans_fixed), 0),
    SD_BUS_WRITABLE_PROPERTY("AcTimeout", "i", NULL, set_timeouts, offsetof(dimmer_conf_t, timeout[ON_AC]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattTimeout", "i", NULL, set_timeouts, offsetof(dimmer_conf_t, timeout[ON_BATTERY]), 0),
    SD_BUS_VTABLE_END
};

DECLARE_MSG(display_req, DISPLAY_REQ);
DECLARE_MSG(bl_req, BL_REQ);

API(Dimmer, conf_dimmer_vtable, conf.dim_conf);
MODULE_WITH_PAUSE("DIMMER");

static void init(void) {
    M_SUB(UPOWER_UPD);
    M_SUB(INHIBIT_UPD);
    M_SUB(SUSPEND_UPD);
    M_SUB(DIMMER_TO_REQ);
    M_SUB(SIMULATE_REQ);
    m_become(waiting_acstate);
    
    init_Dimmer_api();
}

static bool check(void) {
    return true;
}

static bool evaluate(void) {
    return !conf.dim_conf.disabled;
}

static void receive_waiting_acstate(const msg_t *msg, UNUSED const void *userdata) {
    switch (MSG_TYPE()) {
        case UPOWER_UPD: {
            int r = idle_init(client, &slot, on_new_idle);
            if (r != 0) {
                WARN("Failed to init. Killing module.\n");
                module_deregister((self_t **)&self());
            } else {
                m_unbecome();

                // Eventually pause dimmer if initial timeout is <= 0, else set the initial timeout
                timeout_callback();
            }
            break;
        }
        default:
            break;
    }
}

static void receive(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
    case UPOWER_UPD:
        timeout_callback();
        break;
    case INHIBIT_UPD:
        pause_dimmer(state.inhibited, INHIBIT);
        break;
    case SUSPEND_UPD:
        pause_dimmer(state.suspended, SUSPEND);
        break;
    case DIMMER_TO_REQ: {
        timeout_upd *up = (timeout_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            conf.dim_conf.timeout[up->state] = up->new;
            if (up->state == state.ac_state) {
                timeout_callback();
            }
        }
        break;
    }
    case SIMULATE_REQ: {
        /* Validation is useless here; only for coherence */
        if (VALIDATE_REQ((void *)msg->ps_msg->message)) {
            idle_client_reset(client, conf.dim_conf.timeout[state.ac_state]);
        }
        break;
    }
    default:
        break;
    }
}

static void receive_paused(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
    case UPOWER_UPD:
        timeout_callback();
        break;
    case INHIBIT_UPD:
        pause_dimmer(state.inhibited, INHIBIT);
        break;
    case SUSPEND_UPD:
        pause_dimmer(state.suspended, SUSPEND);
        break;
    case DIMMER_TO_REQ: {
        timeout_upd *up = (timeout_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            conf.dim_conf.timeout[up->state] = up->new;
            if (up->state == state.ac_state) {
                timeout_callback();
            }
        }
        break;
    }
    default:
        /* SIMULATE_REQ is not handled while paused */
        break;
    }
}

static void destroy(void) {
    idle_client_destroy(client);
    if (slot) {
        slot = sd_bus_slot_unref(slot);
    }
    deinit_Dimmer_api();
}

static int on_new_idle(sd_bus_message *m, UNUSED void *userdata, UNUSED sd_bus_error *ret_error) {
    int idle;
    
    /* Unused in requests! */
    display_req.display.old = state.display_state;
    sd_bus_message_read(m, "b", &idle);
    if (idle) {
        display_req.display.new = DISPLAY_DIMMED;
    } else {
        display_req.display.new = DISPLAY_ON;
    }
    M_PUB(&display_req);
    return 0;
}

static void timeout_callback(void) {
    if (conf.dim_conf.timeout[state.ac_state] <= 0) {
        pause_dimmer(true, TIMEOUT);
    } else {
        pause_dimmer(false, TIMEOUT);
        idle_set_timeout(client, conf.dim_conf.timeout[state.ac_state]);
    }
}

static void pause_dimmer(const bool pause, enum mod_pause reason) {
    if (CHECK_PAUSE(pause, reason)) {
        if (!pause) {
            idle_client_start(client, conf.dim_conf.timeout[state.ac_state]);
            m_unbecome();
        } else {
            idle_client_stop(client);
            m_become(paused);
        }
    }
}

void dimmer_publish_bl_req(const double pct, enum dim_trans trans) {
    bl_req.bl.new = pct;
    bl_req.bl.smooth = -2 - trans; 
    M_PUB(&bl_req);
}
