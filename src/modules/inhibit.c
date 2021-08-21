#include "interface.h"

static void on_inhibit_req(inhibit_upd *up);

static const sd_bus_vtable conf_inh_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_WRITABLE_PROPERTY("InhibitDocked", "b", NULL, NULL, offsetof(inh_conf_t, inhibit_docked), 0),
    SD_BUS_WRITABLE_PROPERTY("InhibitPM", "b", NULL, NULL, offsetof(inh_conf_t, inhibit_pm), 0),
    SD_BUS_WRITABLE_PROPERTY("InhibitBL", "b", NULL, NULL, offsetof(inh_conf_t, inhibit_bl), 0),
    SD_BUS_VTABLE_END
};

API(Inhibit, conf_inh_vtable, conf.inh_conf);
MODULE("INHIBIT");

static void init(void) {
    M_SUB(INHIBIT_REQ);
    
    init_Inhibit_api();
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
    deinit_Inhibit_api();
}

static void on_inhibit_req(inhibit_upd *up) {
    static int inhibition_ctr;
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
