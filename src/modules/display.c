#include "bus.h"

static void dim_backlight(const double pct);
static void restore_backlight(const double pct);
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
    return !conf.no_dimmer || !conf.no_dpms;
}

static void receive(const msg_t *const msg, UNUSED const void* userdata) {
    static double old_pct = -1.0;
    
    switch (MSG_TYPE()) {
    case DISPLAY_REQ: {
        display_upd *up = (display_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            display_msg.display.old = state.display_state;
                    
            /*
             * If we are now dimmed, dim backlight, else if we are now in dpms, set dpms level.
             * Otherwise we should come back to normal state, ie: DISPLAY_ON.
             * If we were in dpms, set new dpms level (ie: switch ON monitor),
             * plus, if we were dimmed, reset correct backligth level.
             */
            if (up->new == DISPLAY_DIMMED) {
                state.display_state |= DISPLAY_DIMMED;
                DEBUG("Entering dimmed state...\n");
                old_pct = state.current_bl_pct;
                dim_backlight(conf.dimmer_pct);
            } else if (up->new == DISPLAY_OFF) {
                state.display_state |= DISPLAY_OFF;
                DEBUG("Entering dpms state...\n");
                set_dpms(true);
            } else if (up->new == ~DISPLAY_OFF) {
                state.display_state &= ~DISPLAY_OFF;
                DEBUG("Leaving dpms state...\n");
                set_dpms(false);
            } else  if (up->new == ~DISPLAY_DIMMED) {
                DEBUG("Leaving dimmed state...\n");
                state.display_state &= ~DISPLAY_DIMMED;
                if (old_pct >= 0.0) {
                    restore_backlight(old_pct);
                    old_pct = -1.0;
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

static void dim_backlight(const double pct) {
    /* Don't touch backlight if a lower level is already set */
    if (pct >= state.current_bl_pct) {
        DEBUG("A lower than dimmer_pct backlight level is already set. Avoid changing it.\n");
    } else {
        publish_bl_req(pct, !conf.no_smooth_dimmer[ENTER], conf.dimmer_trans_step[ENTER], conf.dimmer_trans_timeout[ENTER]);
    }
}

/* restore previous backlight level */
static void restore_backlight(const double pct) {
    publish_bl_req(pct, !conf.no_smooth_dimmer[EXIT], conf.dimmer_trans_step[EXIT], conf.dimmer_trans_timeout[EXIT]);
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
    call(NULL, NULL, &args, "ssi", state.display, state.xauthority, enable);
}
