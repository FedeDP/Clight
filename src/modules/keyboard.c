#include "interface.h"
#include "utils.h"
#include "my_math.h"

static void receive_waiting_init(const msg_t *const msg, UNUSED const void* userdata);
static void receive_paused(const msg_t *const msg, UNUSED const void* userdata);
static int init_kbd_backlight(void);
static void on_screen_bl_update(bl_upd *up);
static void set_keyboard_level(double level);
static void set_keyboard_timeout(void);
static void on_curve_req(double *regr_points, int num_points, enum ac_states s);
static void pause_kbd(const bool pause, enum mod_pause reason);

static const sd_bus_vtable conf_kbd_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_WRITABLE_PROPERTY("AcTimeout", "i", NULL, set_timeouts, offsetof(kbd_conf_t, timeout[ON_AC]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattTimeout", "i", NULL, set_timeouts, offsetof(kbd_conf_t, timeout[ON_BATTERY]), 0),
    SD_BUS_WRITABLE_PROPERTY("AcPoints", "ad", get_curve, set_curve, offsetof(kbd_conf_t, curve[ON_AC]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattPoints", "ad", get_curve, set_curve, offsetof(kbd_conf_t, curve[ON_BATTERY]), 0),
    SD_BUS_VTABLE_END
};

DECLARE_MSG(kbd_msg, KBD_BL_UPD);
DECLARE_MSG(kbd_req, KBD_BL_REQ);

API(Kbd, conf_kbd_vtable, conf.kbd_conf);
MODULE_WITH_PAUSE("KEYBOARD");

static void init(void) {
    if (init_kbd_backlight() == 0) {
        M_SUB(DISPLAY_UPD);
        M_SUB(BL_UPD);
        M_SUB(KBD_BL_REQ);
        M_SUB(UPOWER_UPD);
        M_SUB(KBD_TO_REQ);
        M_SUB(SUSPEND_UPD);
        M_SUB(KBD_CURVE_REQ);
        m_become(waiting_init);
        
        polynomialfit(NULL, &conf.kbd_conf.curve[ON_AC], "AC keyboard backlight");
        polynomialfit(NULL, &conf.kbd_conf.curve[ON_BATTERY], "BATT keyboard backlight");
        
        init_Kbd_api();
    } else {
        module_deregister((self_t **)&self());
    }
}

static bool check(void) {
    return true;
}

static bool evaluate() {
    return !conf.kbd_conf.disabled;
}


static void receive_waiting_init(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
    case UPOWER_UPD:
        m_unbecome();
        set_keyboard_timeout();
        break;
    default:
        break;
    }
}

static void receive(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
    case BL_UPD: {
        on_screen_bl_update((bl_upd *)MSG_DATA());
        break;
    }
    case KBD_BL_REQ: {
        bl_upd *up = (bl_upd *)MSG_DATA();
        /* Do not change kbd backlight for dimmed state (ie: when entering dimmed state)! */
        if (VALIDATE_REQ(up) && !state.display_state) {
            set_keyboard_level(up->new);
        }
        break;
    }
    case KBD_TO_REQ: {
        timeout_upd *up = (timeout_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            conf.kbd_conf.timeout[up->state] = up->new;
            if (up->state == state.ac_state) {
                set_keyboard_timeout();
            }
        }
        break;
    }
     case DISPLAY_UPD:
        pause_kbd(state.display_state, DISPLAY);
        break;
    case UPOWER_UPD:
        set_keyboard_timeout();
        break;
    case SUSPEND_UPD:
        pause_kbd(state.suspended, SUSPEND);
        break;
    case KBD_CURVE_REQ: {
        curve_upd *up = (curve_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            on_curve_req(up->regression_points, up->num_points, up->state);
        }
        break;
    }
    default:
        break;
    }
}

static void receive_paused(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
    case DISPLAY_UPD:
        pause_kbd(state.display_state, DISPLAY);
        break;
    case UPOWER_UPD:
        set_keyboard_timeout();
        break;
    case SUSPEND_UPD:
        pause_kbd(state.suspended, SUSPEND);
        break;
    case KBD_CURVE_REQ: {
        curve_upd *up = (curve_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            on_curve_req(up->regression_points, up->num_points, up->state);
        }
        break;
    }
    default:
        break;
    }
}

static void destroy(void) {
    deinit_Kbd_api();
}

static int parse_bus_reply(sd_bus_message *reply, const char *member, void *userdata) {
    const char *service_list;
    int r = sd_bus_message_read(reply, "s", &service_list);
    if (r >= 0) {
        // Check if /org/clightd/clightd/KbdBacklight has some nodes (it means we have got kbd backlight)
        if (strstr(service_list, "<node name=")) {
            return 0;
        }
        r = -ENOENT;
    }
    return r;
}

static int init_kbd_backlight(void) {
    SYSBUS_ARG_REPLY(kbd_args, parse_bus_reply, NULL, CLIGHTD_SERVICE, "/org/clightd/clightd/KbdBacklight", "org.freedesktop.DBus.Introspectable", "Introspect");
    int r = call(&kbd_args, NULL);
    INFO("Keyboard backlight calibration %s.\n", r == 0 ? "supported" : "unsupported");
    return r;
}

static void on_screen_bl_update(bl_upd *up) {
    const double new_kbd_pct = get_value_from_curve(up->new, &conf.kbd_conf.curve[state.ac_state]);
    /*
     * Only log for first BL_UPD message received:
     *      * either the one with up->smooth = true
     *      * or the only one sent when conf.bl_conf.smooth is disabled
     */
    if (up->smooth || conf.bl_conf.smooth.no_smooth) {
        INFO("Screen backlight: %.3lf -> Keyboard backlight: %.3lf.\n", up->new, new_kbd_pct);
    }
    kbd_req.bl.new = new_kbd_pct;
    M_PUB(&kbd_req);
}

static void set_keyboard_level(double level) {
    SYSBUS_ARG(kbd_args, CLIGHTD_SERVICE, "/org/clightd/clightd/KbdBacklight", "org.clightd.clightd.KbdBacklight", "Set");
    kbd_msg.bl.old = state.current_kbd_pct;
    if (call(&kbd_args, "d", level) == 0) {
        state.current_kbd_pct = level;
        kbd_msg.bl.new = state.current_kbd_pct;
        M_PUB(&kbd_msg);
    }
}

static void set_keyboard_timeout(void) {
    pause_kbd(conf.kbd_conf.timeout[state.ac_state] <= 0, TIMEOUT);
    if (conf.kbd_conf.timeout[state.ac_state] > 0) {
        SYSBUS_ARG(kbd_args, CLIGHTD_SERVICE, "/org/clightd/clightd/KbdBacklight", "org.clightd.clightd.KbdBacklight", "SetTimeout");
        call(&kbd_args, "i", conf.kbd_conf.timeout[state.ac_state]);
    }
}

/* Callback on "AcCurvePoints" and "BattCurvePoints" bus exposed writable properties */
static void on_curve_req(double *regr_points, int num_points, enum ac_states s) {
    curve_t *c = &conf.kbd_conf.curve[s];
    if (regr_points) {
        memcpy(c->points, 
               regr_points, num_points * sizeof(double));
        c->num_points = num_points;
    }
    polynomialfit(NULL, c, s == ON_AC ? "AC keyboard backlight" : "BATT keyboard backlight");
}

static void pause_kbd(const bool pause, enum mod_pause reason) {
    if (CHECK_PAUSE(pause, reason, "KEYBOARD")) {
        if (!pause) {
            m_unbecome();
            // Set correct level for current backlight
            set_keyboard_level(1.0 - state.current_bl_pct);
        } else {
            // Switch off keyboard backlight
            set_keyboard_level(0.0);
            m_become(paused);
        }
    }
}
