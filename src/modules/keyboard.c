#include "bus.h"
#include "utils.h"

static void receive_waiting_init(const msg_t *const msg, UNUSED const void* userdata);
static void receive_paused(const msg_t *const msg, UNUSED const void* userdata);
static int init_kbd_backlight(void);
static void set_keyboard_level(double level);
static void set_keyboard_timeout(void);
static void pause_kbd(const bool pause, enum mod_pause reason);

DECLARE_MSG(kbd_msg, KBD_BL_UPD);

MODULE_WITH_PAUSE("KEYBOARD");

static void init(void) {
    if (init_kbd_backlight() == 0) {
        M_SUB(DISPLAY_UPD);
        M_SUB(BL_UPD);
        M_SUB(KBD_BL_REQ);
        M_SUB(UPOWER_UPD);
        M_SUB(KBD_TO_REQ);
        M_SUB(SUSPEND_UPD);
        m_become(waiting_init);
    } else {
        m_poisonpill(self());
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
        bl_upd *up = (bl_upd *)MSG_DATA();
        /* Only account for target backlight changes, ie: not step ones */
        if (up->smooth || conf.bl_conf.no_smooth) {
            set_keyboard_level(1.0 - up->new);
        }
        break;
    }
    case KBD_BL_REQ: {
        bl_upd *up = (bl_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
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
        pause_kbd(state.display_state && conf.kbd_conf.dim, DISPLAY);
        break;
    case UPOWER_UPD:
        set_keyboard_timeout();
        break;
    case SUSPEND_UPD:
        pause_kbd(state.suspended, SUSPEND);
        break;
    default:
        break;
    }
}

static void receive_paused(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
    case DISPLAY_UPD:
        pause_kbd(state.display_state && conf.kbd_conf.dim, DISPLAY);
        break;
    case UPOWER_UPD:
        set_keyboard_timeout();
        break;
    case SUSPEND_UPD:
        pause_kbd(state.suspended, SUSPEND);
        break;
    default:
        break;
    }
}

static void destroy(void) {
    
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

static void set_keyboard_level(double level) {
    if (level < 1 - conf.kbd_conf.amb_br_thres) {
        level = 0;
    }
    
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
