#include "bus.h"

static int hook_suspend_signal(void);
static int on_new_suspend(sd_bus_message *m, UNUSED void *userdata, UNUSED sd_bus_error *ret_error);
static int parse_bus_reply(sd_bus_message *reply, const char *member, void *userdata);
static void publish_pm_msg(const bool old, const bool new);
static void on_pm_req(const bool new);
static void on_suspend_req(suspend_upd *up);

MODULE("PM");

static sd_bus_slot *slot;
static unsigned int pm_inh_token;
static int delayed_resume_fd;

static void init(void) {
    pm_inh_token = -1; // UINT MAX
    
    M_SUB(PM_REQ);
    M_SUB(INHIBIT_UPD);
    M_SUB(SUSPEND_REQ);
    
    hook_suspend_signal();
    delayed_resume_fd = start_timer(CLOCK_BOOTTIME, 0, 0);
}

static bool check(void) {
    return true;
}

static bool evaluate() {
    return !conf.inh_conf.disabled;
}

static void receive(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
        case FD_UPD: {
            /* We are reading a message delyaed of conf.resumedelay */
            message_t *suspend_msg = (message_t *)msg->fd_msg->userptr;
            read_timer(delayed_resume_fd);
            M_PUB(suspend_msg);
            m_deregister_fd(delayed_resume_fd);
            break;
        }
        case INHIBIT_UPD: {
            if (conf.inh_conf.inhibit_pm) {
                DECLARE_HEAP_MSG(pm_req, PM_REQ);
                pm_req->pm.old = !state.inhibited;
                pm_req->pm.new = state.inhibited;
                M_PUB(pm_req);
            }
            break;
        }
        case PM_REQ: {
            pm_upd *up = (pm_upd *)MSG_DATA();
            if (VALIDATE_REQ(up)) {
                on_pm_req(up->new);
            }
            break;
        }
        case SYSTEM_UPD:
            /* Release any PowerManagement inhibition upon leaving as we're not PM manager */
            if (msg->ps_msg->type == LOOP_STOPPED && pm_inh_token != -1) {
                on_pm_req(false);
            }
            break;
        case SUSPEND_REQ: {
            suspend_upd *up = (suspend_upd *)MSG_DATA();
            if (VALIDATE_REQ(up)) {
                on_suspend_req(up);
            }
            break;
        }
        default:
            break;
    }
}

static void destroy(void) {
    /* Destroy this match slot */
    if (slot) {
        slot = sd_bus_slot_unref(slot);
    }
    close(delayed_resume_fd);
}

static int hook_suspend_signal(void) {
    SYSBUS_ARG(args, "org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", "PrepareForSleep");
    return add_match(&args, &slot, on_new_suspend);
}

static int on_new_suspend(sd_bus_message *m, UNUSED void *userdata, UNUSED sd_bus_error *ret_error) {
    int going_suspend;
    sd_bus_message_read(m, "b", &going_suspend);
    
    DECLARE_HEAP_MSG(suspend_req, SUSPEND_REQ);
    suspend_req->suspend.new = going_suspend;
    M_PUB(suspend_req);
    return 0;
}

static int parse_bus_reply(sd_bus_message *reply, const char *member, void *userdata) {
    int r = -EINVAL;
    if (!strcmp(member, "Inhibit")) {
        r = sd_bus_message_read(reply, "u", userdata);
    }
    return r;
}

static void publish_pm_msg(const bool old, const bool new) {
    DECLARE_HEAP_MSG(pm_msg, PM_UPD);
    pm_msg->pm.old = old;
    pm_msg->pm.new = new;
    M_PUB(pm_msg);
    state.pm_inhibited = new;
}

static void on_pm_req(const bool new) {
    if (new && pm_inh_token == -1) {
        USERBUS_ARG_REPLY(pm_args, parse_bus_reply, &pm_inh_token,
                            "org.freedesktop.PowerManagement.Inhibit",
                            "/org/freedesktop/PowerManagement/Inhibit",
                            "org.freedesktop.PowerManagement.Inhibit",
                            "Inhibit");
        if (call(&pm_args, "ss", "Clight", "PM inhibition.") == 0) {
            DEBUG("Holding PowerManagement inhibition.\n");
            publish_pm_msg(false, true);
        }
    } else if (!new && pm_inh_token != -1) {
        USERBUS_ARG(pm_args,
                    "org.freedesktop.PowerManagement.Inhibit",  
                    "/org/freedesktop/PowerManagement/Inhibit", 
                    "org.freedesktop.PowerManagement.Inhibit", 
                    "UnInhibit");
        if (call(&pm_args, "u", pm_inh_token) == 0) {
            DEBUG("Released PowerManagement inhibition.\n");
            pm_inh_token = -1;
            publish_pm_msg(true, false);
        }
    }
}

static void on_suspend_req(suspend_upd *up) {
    state.suspended = up->new;

    DECLARE_HEAP_MSG(suspend_msg, SUSPEND_UPD);
    suspend_msg->suspend.new = up->new;
    suspend_msg->suspend.old = !state.suspended;
    
    /* 
     * NOTE: resuming from suspend can be delayed up to 30s 
     * because in some reported cases, clight is too fast to
     * sync screen temperature, failing because Xorg is still not fully resumed. 
     */
    if (!up->new && conf.resumedelay > 0) {
        set_timeout(conf.resumedelay, 0, delayed_resume_fd, 0);
        m_register_fd(delayed_resume_fd, false, suspend_msg);
    } else {
        M_PUB(suspend_msg);
    }
}
