#include "bus.h"

DECLARE_MSG(inh_msg, INHIBIT_UPD);

MODULE("INHIBIT");

static int inhibition_ctr;

static void init(void) {
    M_SUB(INHIBIT_REQ);
}

static bool check(void) {
    return true;
}

static bool evaluate() {
    return true;
}

static void receive(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
    case INHIBIT_REQ: {
        inhibit_upd *up = (inhibit_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            /* Drop an inhibition from our counter */
            if (!up->new) {
                inhibition_ctr--;
            }
            
            if (up->new || inhibition_ctr == 0) {
                inh_msg.inhibit.old = state.inhibited;
                state.inhibited = up->new;
                inh_msg.inhibit.new = state.inhibited;
                M_PUB(&inh_msg);
            }
            INFO("A Clight inhibition has been %s: '%s'.\n", state.inhibited ? "enabled" : "disabled", 
                 up->reason ? up->reason : "no reason specified.");
        }
        /* Count currently held inhibitions */
        if (up->new) {
            inhibition_ctr++;
        }
        free((void *)up->reason);
        break;
    }
    default:
        break;
    }
}

static void destroy(void) {

}
