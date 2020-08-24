#include "bus.h"

static int init_kbd_backlight(void);
static void set_keyboard_level(double level);
static void dimmed_callback(void);

DECLARE_MSG(kbd_msg, KBD_BL_UPD);

MODULE("KEYBOARD");

static int max_kbd_backlight;

static void init(void) {
    if (init_kbd_backlight() == 0 && max_kbd_backlight > 0) {
        M_SUB(DISPLAY_UPD);
        M_SUB(AMBIENT_BR_UPD);
        M_SUB(KBD_BL_REQ);
        
        /* Switch off keyboard from start as BACKLIGHT sets 100% backlight */
        if (!conf.bl_conf.disabled && conf.bl_conf.no_auto_calib) {
            set_keyboard_level(1.0);
        }
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

static void receive(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
    case DISPLAY_UPD:
        dimmed_callback();
        break;
    case AMBIENT_BR_UPD:
        set_keyboard_level((conf.kbd_conf.amb_br_thres - state.ambient_br) / conf.kbd_conf.amb_br_thres);
        break;
    case KBD_BL_REQ: {
        bl_upd *up = (bl_upd *)MSG_DATA();
        if (VALIDATE_REQ(up) && !state.display_state) {
            set_keyboard_level(up->new);
        }
        break;
    }
    default:
        break;
    }
}

static void destroy(void) {
    
}

static int parse_bus_reply(sd_bus_message *reply, const char *member, void *userdata) {    
    int r = -EINVAL;
    if (!strcmp(member, "GetMaxBrightness")) {
        r = sd_bus_message_read(reply, "i", &max_kbd_backlight);
    }
    return r;
}

static int init_kbd_backlight(void) {
    SYSBUS_ARG_REPLY(kbd_args, parse_bus_reply, NULL, "org.freedesktop.UPower", "/org/freedesktop/UPower/KbdBacklight", "org.freedesktop.UPower.KbdBacklight", "GetMaxBrightness");
    int r = call(&kbd_args, NULL);
    INFO("Keyboard backlight calibration %s.\n", !r && max_kbd_backlight > 0 ? "supported" : "unsupported");
    return r;
}

static void set_keyboard_level(double level) {
    if (level < 0) {
        level = 0;
    }

    SYSBUS_ARG(kbd_args, "org.freedesktop.UPower", "/org/freedesktop/UPower/KbdBacklight", "org.freedesktop.UPower.KbdBacklight", "SetBrightness");
    kbd_msg.bl.old = state.current_kbd_pct;
    /* We actually need to pass an int to variadic bus() call */
    const int new_kbd_br = round(level * max_kbd_backlight);
    if (call(&kbd_args, NULL, "i", new_kbd_br) == 0) {
        state.current_kbd_pct = level;
        kbd_msg.bl.new = state.current_kbd_pct;
        M_PUB(&kbd_msg);
    }
}

/* Callback on state.display_state changes */
static void dimmed_callback(void) {
    static double old_kbd_level = -1.0;
    
    if (state.display_state) {
        /* Switch off keyboard if requested */
        if (conf.kbd_conf.dim) {
            old_kbd_level = state.current_kbd_pct;
            set_keyboard_level(0.0);
        }
    } else {
        /* Reset keyboard backlight level if needed */
        if (old_kbd_level != -1.0) {
            set_keyboard_level(old_kbd_level);
            old_kbd_level = -1.0;
        }
    }
}
