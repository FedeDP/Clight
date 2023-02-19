#include "bus.h"
#include <fcntl.h>

static int hook_suspend_signal(void);
static int session_active_listener_init(void);
static int on_new_suspend(sd_bus_message *m, UNUSED void *userdata, UNUSED sd_bus_error *ret_error);
static int on_session_change(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int parse_bus_reply(sd_bus_message *reply, const char *member, void *userdata);
static void publish_pm_msg(const bool old, const bool new);
static void publish_susp_req(const bool new);
static void on_pm_req(const bool new);
static void on_suspend_req(suspend_upd *up);

MODULE("PM");

static sd_bus_slot *slot;
static unsigned int pm_inh_token;
static int delayed_resume_fd;

static enum
{
  NONE = 0,
  SYSTEMD,
  POWERMANAGEMENT
} inh_api;

static void init(void) {
    pm_inh_token = -1; // UINT MAX
    
    M_SUB(PM_REQ);
    M_SUB(INHIBIT_UPD);
    M_SUB(SUSPEND_REQ);
    
    hook_suspend_signal();
    session_active_listener_init();
    
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
            /* We are reading a message delayed of conf.resumedelay */
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
            // VALIDATE_REQ is called inside
            on_suspend_req(up);
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

static int session_active_listener_init(void) {
    SYSBUS_ARG(args, "org.freedesktop.login1",  "/org/freedesktop/login1", "org.freedesktop.DBus.Properties", "PropertiesChanged");
    return add_match(&args, &slot, on_session_change);
}

static int on_new_suspend(sd_bus_message *m, UNUSED void *userdata, UNUSED sd_bus_error *ret_error) {
    int going_suspend;
    sd_bus_message_read(m, "b", &going_suspend);
    
    publish_susp_req(going_suspend);
    return 0;
}

/* Listener on logind session.Active for current session */
static int on_session_change(UNUSED sd_bus_message *m, UNUSED void *userdata, UNUSED sd_bus_error *ret_error) {
    static int active = 1;
    SYSBUS_ARG(args, "org.freedesktop.login1",  "/org/freedesktop/login1/session/auto", "org.freedesktop.login1.Session", "Active");
    int old_active = active;
    int r = get_property(&args, "b", &active);
    if (!r) {
        if (active != old_active) {
            publish_susp_req(!active);
        }
    }
    return 0;
}

static int parse_bus_reply(sd_bus_message *reply, const char *member,
                           void *userdata) {
    switch (inh_api) {
    case SYSTEMD: {
        unsigned int f;
        int r = sd_bus_message_read(reply, "h", &f);
        if (r >= 0) {
            pm_inh_token = fcntl(f, F_DUPFD_CLOEXEC, 3);
        }
        return r;
    }
    case POWERMANAGEMENT: {
        return sd_bus_message_read(reply, "u", userdata);
    }
    case NONE: {
        return -EINVAL;
    }
    }
}

static void publish_pm_msg(const bool old, const bool new) {
    DECLARE_HEAP_MSG(pm_msg, PM_UPD);
    pm_msg->pm.old = old;
    pm_msg->pm.new = new;
    M_PUB(pm_msg);
    state.pm_inhibited = new;
}

static void publish_susp_req(const bool new) {
    DECLARE_HEAP_MSG(suspend_req, SUSPEND_REQ);
    suspend_req->suspend.new = new;
    M_PUB(suspend_req);
}

static bool acquire_systemd_lock(void) {
    inh_api = SYSTEMD;
    SYSBUS_ARG_REPLY(pm_args, parse_bus_reply, &pm_inh_token,
                     "org.freedesktop.login1", "/org/freedesktop/login1",
                     "org.freedesktop.login1.Manager", "Inhibit");
    int ret = call(&pm_args, "ssss", "idle", "Clight", "Idle inhibitor.", "block");
    if (ret == 0 && pm_inh_token > 0) {
        DEBUG("Holding inhibition with systemd-inhibit.\n");
        return true;
    }
    
    inh_api = NONE;
    if (ret < 0) {
        DEBUG("Failed to parse systemd-inhibit D-Bus response.\n");
    } else {
        DEBUG("Failed to copy lock file\n");
    }
    return false;
}

static bool acquire_pm_lock(void) {
    inh_api = POWERMANAGEMENT;
    SYSBUS_ARG_REPLY(pm_args, parse_bus_reply, &pm_inh_token,
                     "org.freedesktop.PowerManagement.Inhibit",
                     "/org/freedesktop/PowerManagement/Inhibit",
                     "org.freedesktop.PowerManagement.Inhibit", 
                     "Inhibit");
    if (call(&pm_args, "ss", "Clight", "Idle inhibitor.", "block") == 0) {
        DEBUG("Holding inhibition with PowerManagement.\n");
        return true;
    }
    inh_api = NONE;
    DEBUG("Failed to parse PowerManagement D-Bus response.\n");
    return false;
}

static bool release_lock(void) {
    switch (inh_api) {
    case SYSTEMD: {
        close(pm_inh_token);
        DEBUG("Released systemd-inhibit idle inhibition.\n");
        pm_inh_token = -1;
        return true;
    }
    case POWERMANAGEMENT: {
        USERBUS_ARG(pm_args, "org.freedesktop.PowerManagement.Inhibit",
                    "/org/freedesktop/PowerManagement/Inhibit",
                    "org.freedesktop.PowerManagement.Inhibit", "UnInhibit");
        if (call(&pm_args, "u", pm_inh_token) == 0) {
            DEBUG("Released PowerManagement idle inhibition.\n");
            pm_inh_token = -1;
            return true;
        }
    }
    case NONE:
        return false;
    }
}

static void on_pm_req(const bool new) {
    if (new && pm_inh_token == -1) {
        if (acquire_systemd_lock() || acquire_pm_lock()) {
            publish_pm_msg(false, true);
        }
    } else if (!new && pm_inh_token != -1) {
        if (release_lock()) {
            publish_pm_msg(true, false);
            inh_api = NONE;
        }
    }
}

static void on_suspend_req(suspend_upd *up) {
    static int suspend_ctr = 0;
    if (VALIDATE_REQ(up)) {
        /* Drop a suspend source from our counter */
        if (!up->new) {
            if (!up->force) {
                suspend_ctr--;
            } else {
                suspend_ctr = 0;
            }
        }
        
        if (up->new || suspend_ctr == 0) {
            DECLARE_HEAP_MSG(suspend_msg, SUSPEND_UPD);
            suspend_msg->suspend.old = state.suspended;
            state.suspended = up->new;
            suspend_msg->suspend.new = up->new;
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
    }
    /* Count currently held suspends */
    if (up->new) {
        suspend_ctr++;
    }
    DEBUG("Suspend ctr: %d\n", suspend_ctr);
}
