#include "bus.h"

static void on_inhibit_req(inhibit_upd *up);

DECLARE_MSG(inh_msg, INHIBIT_UPD);

MODULE("INHIBIT");

static int inhibition_ctr;

static void init(void) {
    M_SUB(INHIBIT_REQ);

    /* Are we starting DOCKED? Inhibit immediately! */
    if (state.lid_state == DOCKED) {
        inhibit_upd req = { false, true, false, NULL, strdup("Docked laptop.") };
        on_inhibit_req(&req);
    }
}

static bool check(void) {
    return true;
}

static bool evaluate() {
    return (!conf.dim_conf.disabled || !conf.dpms_conf.disabled) && state.lid_state != -1; // start once we have a lid state, to check for DOCKED state
}

static void receive(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
    case INHIBIT_REQ: {
        inhibit_upd *up = (inhibit_upd *)MSG_DATA();
        on_inhibit_req(up);
        break;
    }
    default:
        break;
    }
}

static void destroy(void) {

}

static void on_inhibit_req(inhibit_upd *up) {
    if (VALIDATE_REQ(up)) {
        /* Drop an inhibition from our counter */
        if (!up->new) {
            if (!up->force) {
                inhibition_ctr--;
                INFO("Clight inhibition released by '%s'.\n", up->app_name ? up->app_name : "Clight");
            } else {
                inhibition_ctr = 0;
                INFO("Clight inhibition forcefully cleared by '%s'.\n", up->app_name ? up->app_name : "Clight");
            }
        }
        
        if (up->new || inhibition_ctr == 0) {
            inh_msg.inhibit.old = state.inhibited;
            state.inhibited = up->new;
            inh_msg.inhibit.new = state.inhibited;
            M_PUB(&inh_msg);
        }
    }
    /* Count currently held inhibitions */
    if (up->new) {
        inhibition_ctr++;
        INFO("New Clight inhibition held by '%s': '%s'\n", 
             up->app_name ? up->app_name : "Clight",
             up->reason ? up->reason : "no reason specified.");
    }
    free((void *)up->app_name);
    free((void *)up->reason);
}
