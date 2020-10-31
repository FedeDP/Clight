#include "bus.h"
#include "utils.h"

static void publish_bl_req(const double pct, const bool smooth, const double step, const int to);
static void set_dpms(bool enable);

DECLARE_MSG(display_msg, DISPLAY_UPD);
DECLARE_MSG(bl_req, BL_REQ);

MODULE("DISPLAY");

static void init(void) {
    M_SUB(DISPLAY_REQ);
}

static bool check(void) {
    return true;
}

static bool evaluate(void) {
    return !conf.dim_conf.disabled || !conf.dpms_conf.disabled;
}

static void receive(const msg_t *const msg, UNUSED const void* userdata) {
    static double old_pct = -1.0;
    
    switch (MSG_TYPE()) {
    case DISPLAY_REQ: {
        display_upd *up = (display_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            display_msg.display.old = state.display_state;
            if (up->new == DISPLAY_DIMMED) {
                state.display_state |= DISPLAY_DIMMED;
                DEBUG("Entering dimmed state...\n");
                if (state.current_bl_pct > conf.dim_conf.dimmed_pct) {
                    old_pct = state.current_bl_pct;
                    publish_bl_req(conf.dim_conf.dimmed_pct, !conf.dim_conf.no_smooth[ENTER], 
                                   conf.dim_conf.trans_step[ENTER], conf.dim_conf.trans_timeout[ENTER]);
                } else {
                    DEBUG("A lower than dimmer_pct backlight level is already set. Avoid changing it.\n");
                }
            } else if (up->new == DISPLAY_OFF) {
                state.display_state |= DISPLAY_OFF;
                DEBUG("Entering dpms state...\n");
                set_dpms(true);
            } else if (up->new == DISPLAY_ON) {
                if (state.display_state & DISPLAY_OFF) {
                    state.display_state &= ~DISPLAY_OFF;
                    DEBUG("Leaving dpms state...\n");
                    set_dpms(false);
                }
                if (state.display_state & DISPLAY_DIMMED) {
                    state.display_state &= ~DISPLAY_DIMMED;
                    DEBUG("Leaving dimmed state...\n");
                    if (old_pct >= 0.0) {
                        publish_bl_req(old_pct, !conf.dim_conf.no_smooth[EXIT], 
                                       conf.dim_conf.trans_step[EXIT], conf.dim_conf.trans_timeout[EXIT]);
                        old_pct = -1.0;
                    }
                }
            }
            display_msg.display.new = state.display_state;
            M_PUB(&display_msg);
        }
        break;
    }
    default:
        break;
    }
}

static void destroy(void) {

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
