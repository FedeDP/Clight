#include "bus.h"
#include <module/queue.h>

static int parse_bus_reply(sd_bus_message *reply, const char *member, void *userdata);
static void on_pm_req(pm_upd *up);

DECLARE_MSG(pm_req, PM_REQ);

MODULE("PM");

static unsigned int pm_inh_token;

static void init(void) {
    pm_inh_token = -1; // UINT MAX
    
    M_SUB(PM_REQ);
    M_SUB(INHIBIT_UPD);

    /* Are we starting inhibited? React immediately! */
    if (state.inhibited && conf.inh_conf.inhibit_pm) {
        pm_req.pm.new = true;
        on_pm_req(&pm_req.pm);
    }
}

static bool check(void) {
    return true;
}

static bool evaluate() {
    return !conf.inh_conf.disabled;
}

static void receive(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
        case INHIBIT_UPD:
            if (conf.inh_conf.inhibit_pm) {
                pm_req.pm.new = state.inhibited;
                M_PUB(&pm_req);
            }
            break;
        case PM_REQ: {
            pm_upd *up = (pm_upd *)MSG_DATA();
            on_pm_req(up);
            break;
        }
        case SYSTEM_UPD:
            /* Release any PowerManagement inhibition upon leaving as we're not PM manager */
            if (msg->ps_msg->type == LOOP_STOPPED && pm_inh_token != -1) {
                pm_req.pm.new = false;
                on_pm_req(&pm_req.pm);
            }
            break;
        default:
            break;
    }
}

static void destroy(void) {

}

static int parse_bus_reply(sd_bus_message *reply, const char *member, void *userdata) {
    int r = -EINVAL;
    if (!strcmp(member, "Inhibit")) {
        r = sd_bus_message_read(reply, "u", userdata);
    }
    return r;
}

static void on_pm_req(pm_upd *up) {
    if (VALIDATE_REQ(up)) {
        if (up->new && pm_inh_token == -1) {
            USERBUS_ARG_REPLY(pm_args, parse_bus_reply, &pm_inh_token,
                              "org.freedesktop.PowerManagement.Inhibit",  
                              "/org/freedesktop/PowerManagement/Inhibit", 
                              "org.freedesktop.PowerManagement.Inhibit", 
                              "Inhibit");
            if (call(&pm_args, "ss", "Clight", "PM inhibition.") == 0) {
                INFO("Holding PowerManagement inhibition.\n");
            }
        } else if (!up->new && pm_inh_token != -1) {
            USERBUS_ARG(pm_args,
                        "org.freedesktop.PowerManagement.Inhibit",  
                        "/org/freedesktop/PowerManagement/Inhibit", 
                        "org.freedesktop.PowerManagement.Inhibit", 
                        "UnInhibit");
            if (call(&pm_args, "u", pm_inh_token) == 0) {
                INFO("Released PowerManagement inhibition.\n");
                pm_inh_token = -1;
            }
        }
    }
}
