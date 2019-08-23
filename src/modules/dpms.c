#include "idler.h"
#include <bus.h>

static int on_new_idle(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static void set_dpms(int dpms_state);
static void upower_timeout_callback(void);
static void inhibit_callback(void);

static sd_bus_slot *slot;
static char client[PATH_MAX + 1];
static display_upd display_msg = { DISPLAY_UPD };

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

static void receive(const msg_t *const msg, const void* userdata) {
    if (msg->is_pubsub && msg->ps_msg->type == USER) {
        MSG_TYPE();
        switch (type) {
        case UPOWER_UPD:
            upower_timeout_callback();
            break;
        case DPMS_TO_REQ: {
            timeout_upd *up = (timeout_upd *)msg->ps_msg->message;
            conf.dpms_timeout[up->state] = up->new;
            if (up->state == state.ac_state) {
                upower_timeout_callback();
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

static int on_new_idle(sd_bus_message *m,  UNUSED void *userdata, UNUSED sd_bus_error *ret_error) {   
    int is_dpms;
    sd_bus_message_read(m, "b", &is_dpms);
    
    display_msg.old = state.display_state;
    
    if (is_dpms) {
        state.display_state |= DISPLAY_OFF;
        DEBUG("Entering dpms state...\n");
    } else {
        state.display_state &= ~DISPLAY_OFF;
        DEBUG("Leaving dpms state...\n");
    }
    display_msg.new = state.display_state;
    set_dpms(is_dpms);
    M_PUB(&display_msg);
    return 0;
}

static void set_dpms(int dpms_state) {
    SYSBUS_ARG(args, CLIGHTD_SERVICE, "/org/clightd/clightd/Dpms", "org.clightd.clightd.Dpms", "Set");
    call(NULL, NULL, &args, "ssi", state.display, state.xauthority, dpms_state);
}

static void upower_timeout_callback(void) {
    idle_set_timeout(client, conf.dpms_timeout[state.ac_state]);
}

/*
 * If we're getting inhibited, stop idle client.
 * Else, restart it.
 */
static void inhibit_callback(void) {
    if (!state.pm_inhibited) {
        DEBUG("Being resumed.\n");
        idle_client_start(client);
    } else {
        DEBUG("Being paused.\n");
        idle_client_stop(client);
    }
}
