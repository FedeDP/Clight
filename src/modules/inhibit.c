#include "bus.h"

DECLARE_MSG(inh_msg, INHIBIT_UPD);

MODULE("INHIBIT");

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
            inh_msg.inhibit.old = state.inhibited;
            state.inhibited = up->new;
            inh_msg.inhibit.new = state.inhibited;
            M_PUB(&inh_msg);
            INFO("Clight inhibition %s: '%s'.\n", state.inhibited ? "enabled" : "disabled", 
                 up->reason ? up->reason : "no reason specified.");
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
