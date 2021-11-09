#include "bus.h"
#include "utils.h"

static int hook_logind_idle_signal(void);
static int on_idle_hint_changed(sd_bus_message *m, UNUSED void *userdata, UNUSED sd_bus_error *ret_error);
static void publish_bl_req(const double pct, const bool smooth, const double step, const int to);
static void set_dpms(bool enable);

static sd_bus_slot *slot;

DECLARE_MSG(display_msg, DISPLAY_UPD);
DECLARE_MSG(bl_req, BL_REQ);
DECLARE_MSG(display_req, DISPLAY_REQ);

MODULE("DISPLAY");

static void init(void) {
    M_SUB(DISPLAY_REQ);
    
    /* 
     * Try to hook logind IdleHint property, only if dimmer mod is disabled:
     * this guarantees us that only a single DIMMED state producer is enabled.
     * Moreover, when dimmer module is enabled, there is no real use of managing IdleHint.
     * Note that we won't touch backlight as user disabled DIMMER module;
     * we will only manage display state as dimmed, eventually pausing backlight and other modules.
     */
    if (conf.dim_conf.disabled) {
        hook_logind_idle_signal();
    }
}

static bool check(void) {
    return true;
}

static bool evaluate(void) {
    return true;
}

static void receive(const msg_t *const msg, UNUSED const void* userdata) {
    static double old_pct = -1.0;
    
    switch (MSG_TYPE()) {
    case DISPLAY_REQ: {
        display_upd *up = (display_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            display_msg.display.old = state.display_state;
            switch (up->new) {
            case DISPLAY_DIMMED:
                state.display_state |= DISPLAY_DIMMED;
                DEBUG("Entering dimmed state...\n");
                /*
                 * If the message was sent by ourself, it means it is an IdleHint and DIMMER mod is disabled.
                 * Just set the dimmed state without touching backlight.
                 */
                if (msg->ps_msg->sender != self()) {
                    // Message arrives from DIMMER module (or external plugin)
                    if (state.current_bl_pct > conf.dim_conf.dimmed_pct) {
                        old_pct = state.current_bl_pct;
                        publish_bl_req(conf.dim_conf.dimmed_pct, !conf.dim_conf.no_smooth[ENTER], 
                                        conf.dim_conf.trans_step[ENTER], conf.dim_conf.trans_timeout[ENTER]);
                    } else {
                        DEBUG("A lower than dimmer_pct backlight level is already set. Avoid changing it.\n");
                    }
                }
                break;
            case DISPLAY_OFF:
                state.display_state |= DISPLAY_OFF;
                DEBUG("Entering dpms state...\n");
                set_dpms(true);
                break;
            case DISPLAY_ON:
                if (state.display_state & DISPLAY_OFF) {
                    state.display_state &= ~DISPLAY_OFF;
                    DEBUG("Leaving dpms state...\n");
                    set_dpms(false);
                }
                if (state.display_state & DISPLAY_DIMMED) {
                    state.display_state &= ~DISPLAY_DIMMED;
                    DEBUG("Leaving dimmed state...\n");
                    if (msg->ps_msg->sender != self() && old_pct >= 0.0) {
                        publish_bl_req(old_pct, !conf.dim_conf.no_smooth[EXIT], 
                                       conf.dim_conf.trans_step[EXIT], conf.dim_conf.trans_timeout[EXIT]);
                        old_pct = -1.0;
                    }
                }
                break;
            default:
                break;
            }
            display_msg.display.new = state.display_state;
            M_PUB(&display_msg);
            
            /* Properly set the IdleHint on logind where available */
            SYSBUS_ARG(args, "org.freedesktop.login1", "/org/freedesktop/login1/session/auto", "org.freedesktop.login1.Session", "SetIdleHint");
            call(&args, "b", state.display_state);
        }
        break;
    }
    default:
        break;
    }
}

static void destroy(void) {
    if (slot) {
        slot = sd_bus_slot_unref(slot);
    }
}

static int hook_logind_idle_signal(void) {
    SYSBUS_ARG(args, "org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.DBus.Properties", "PropertiesChanged");
    return add_match(&args, &slot, on_idle_hint_changed);
}

static int on_idle_hint_changed(sd_bus_message *m, UNUSED void *userdata, UNUSED sd_bus_error *ret_error) {
    int idle;
    
    /* Unused in requests! */
    display_req.display.old = state.display_state;
    // This comes from logind!
    SYSBUS_ARG(idle_args, "org.freedesktop.login1",  "/org/freedesktop/login1/session/auto", "org.freedesktop.login1.Session", "IdleHint");
    get_property(&idle_args, "b", &idle);
    if (idle) {
        display_req.display.new = DISPLAY_DIMMED;
    } else {
        display_req.display.new = DISPLAY_ON;
    }
    M_PUB(&display_req);
    return 0;
}

static void publish_bl_req(const double pct, const bool smooth, const double step, const int to) {
    bl_req.bl.new = pct;
    bl_req.bl.smooth = smooth;
    bl_req.bl.step = step;
    bl_req.bl.timeout = to;
    M_PUB(&bl_req);
}

static void set_dpms(bool enable) {
    SYSBUS_ARG(args, CLIGHTD_SERVICE, "/org/clightd/clightd/Dpms", "org.clightd.clightd.Dpms", "Set");
    call(&args, "ssi", fetch_display(), fetch_env(), enable);
}
