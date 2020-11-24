#include "bus.h"

static void on_inhibit_req(inhibit_upd *up);

MODULE("INHIBIT");

static int inhibition_ctr;

static void init(void) {
    M_SUB(INHIBIT_REQ);
}

static bool check(void) {
    return true;
}

static bool evaluate() {
    return !conf.inh_conf.disabled;
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
                DEBUG("ScreenSaver inhibition released.\n");
            } else {
                inhibition_ctr = 0;
                DEBUG("ScreenSaver inhibition forcefully cleared.\n");
            }
        }
        
        if (up->new || inhibition_ctr == 0) {
            DECLARE_HEAP_MSG(inh_msg, INHIBIT_UPD);
            inh_msg->inhibit.old = state.inhibited;
            state.inhibited = up->new;
            inh_msg->inhibit.new = state.inhibited;
            M_PUB(inh_msg);
        }
    }
    /* Count currently held inhibitions */
    if (up->new) {
        inhibition_ctr++;
        DEBUG("ScreenSaver inhibition grabbed.\n");
    }
    DEBUG("Inhibitions ctr: %d\n", inhibition_ctr);
}
