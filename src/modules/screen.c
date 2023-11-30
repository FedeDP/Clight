#include <limits.h>
#include "interface.h"
#include "my_math.h"
#include "utils.h"

static void receive_waiting_state(const msg_t *msg, UNUSED const void *userdata);
static int parse_bus_reply(sd_bus_message *reply, const char *member, void *userdata);
static int get_screen_brightness(bool emit);
static void timeout_callback(int old_val, bool reset);
static void pause_screen(bool pause, enum mod_pause type, bool reset_screen_br);
static int set_contrib(sd_bus *bus, const char *path, const char *interface, const char *property,
                              sd_bus_message *value, void *userdata, sd_bus_error *error);

static int screen_fd = -1;
static enum msg_type curr_msg;
static const sd_bus_vtable conf_screen_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_WRITABLE_PROPERTY("Contrib", "d", NULL, set_contrib, offsetof(screen_conf_t, contrib), 0),
    SD_BUS_WRITABLE_PROPERTY("AcTimeout", "i", NULL, set_timeouts, offsetof(screen_conf_t, timeout[ON_AC]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattTimeout", "i", NULL, set_timeouts, offsetof(screen_conf_t, timeout[ON_BATTERY]), 0),
    SD_BUS_VTABLE_END
};

DECLARE_MSG(contrib_req, CONTRIB_REQ);
DECLARE_MSG(screen_msg, SCREEN_BR_UPD);

API(Screen, conf_screen_vtable, conf.screen_conf);
MODULE_WITH_PAUSE("SCREEN");

static void init(void) {
    M_SUB(AMBIENT_BR_UPD);
    M_SUB(SCR_TO_REQ);
    M_SUB(UPOWER_UPD);
    M_SUB(DISPLAY_UPD);
    M_SUB(SENS_UPD);
    M_SUB(LID_UPD);
    M_SUB(SUSPEND_UPD);
    M_SUB(NO_AUTOCALIB_UPD);
    M_SUB(INHIBIT_UPD);
    M_SUB(CONTRIB_REQ);
    
    if (get_screen_brightness(false) != 0) {
         // We are on an unsupported wayland compositor; kill ourself immediately without further message processing
        WARN("Failed to init. Killing module.\n");
        module_deregister((self_t **)&self());
    } else {
        m_become(waiting_state);
        init_Screen_api();
    }
}

static bool check(void) {
    return true;
}

static bool evaluate(void) {
    return !conf.screen_conf.disabled;
}

static void destroy(void) {
    if (screen_fd >= 0) {
        close(screen_fd);
    }
    deinit_Screen_api();
}

static void receive_waiting_state(const msg_t *msg, UNUSED const void *userdata) {
    switch (MSG_TYPE()) {
    case UPOWER_UPD: {
        m_unbecome();
        
        /* Start paused if screen timeout for current ac state is <= 0 */
        screen_fd = start_timer(CLOCK_BOOTTIME, conf.screen_conf.timeout[state.ac_state], 0);
        m_register_fd(screen_fd, false, NULL);
        timeout_callback(-1, false);
        
        pause_screen(conf.screen_conf.contrib == 0.0f, CONTRIB, true);
        break;
    }
    default:
        break;
    }
}

static void receive(const msg_t *msg, UNUSED const void *userdata) {
    curr_msg = MSG_TYPE();
    switch (MSG_TYPE()) {
    case AMBIENT_BR_UPD:
        pause_screen(state.ambient_br < conf.bl_conf.shutter_threshold, CLOGGED, false);
        if (!paused_state) {
            get_screen_brightness(true);
        }
        break;
    case FD_UPD:
        read_timer(screen_fd);
        get_screen_brightness(true);
        set_timeout(conf.screen_conf.timeout[state.ac_state], 0, screen_fd, 0);
        break;
    case UPOWER_UPD: {
        upower_upd *up = (upower_upd *)MSG_DATA();
        timeout_callback(conf.screen_conf.timeout[up->old], true);
        break;
    }
    case DISPLAY_UPD:
        pause_screen(state.display_state, DISPLAY, false);
        break;
    case SENS_UPD:
        pause_screen(!state.sens_avail, SENSOR, true);
        break;
    case LID_UPD:
        if (conf.bl_conf.pause_on_lid_closed) {
            pause_screen(state.lid_state, LID, false);
        }
        break;
    case SUSPEND_UPD:
         pause_screen(state.suspended, SUSPEND, false);
         break;
    case SCR_TO_REQ: {
        timeout_upd *up = (timeout_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            const int old = conf.screen_conf.timeout[up->state];
            conf.screen_conf.timeout[up->state] = up->new;
            if (up->state == state.ac_state) {
                timeout_callback(old, true);
            }
        }
        break;
    }
    case NO_AUTOCALIB_UPD: {
        calib_upd *up = (calib_upd *)MSG_DATA();
        pause_screen(up->new, AUTOCALIB, true);
        break;
    }
    case INHIBIT_UPD: {
        pause_screen(state.inhibited && conf.inh_conf.inhibit_bl, INHIBIT, true);
        break;
    }
    case CONTRIB_REQ: {
        contrib_upd *up = (contrib_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            conf.screen_conf.contrib = up->new;
            pause_screen(conf.screen_conf.contrib == 0.0f, CONTRIB, true);
            // Refresh current screen brightness with new contrib!
            if (!paused_state) {
                M_PUB(&screen_msg);
            }
        }
        break;
    }
    default:
        break;
    }
}

static int parse_bus_reply(sd_bus_message *reply, const char *member, void *userdata) {
    int r = -EINVAL;
    if (!strcmp(member, "GetEmittedBrightness")) {
        r = sd_bus_message_read(reply, "d", userdata);
    }
    return r;
}

static int get_screen_brightness(bool emit) {
    if (paused_state) {
        return 0;
    }

    double new_br = 0.0f;
    SYSBUS_ARG_REPLY(args, parse_bus_reply, &new_br, CLIGHTD_SERVICE, "/org/clightd/clightd/Screen", "org.clightd.clightd.Screen", "GetEmittedBrightness");
    
    screen_msg.bl.old = state.screen_br;
    int ret = call(&args, "ss", fetch_display(), fetch_env());
    if (ret == 0 && emit) {
        state.screen_br = new_br;
        screen_msg.bl.new = state.screen_br;
        M_PUB(&screen_msg);
    }
    return ret;
}

static void timeout_callback(int old_val, bool reset) {
    if (conf.screen_conf.timeout[state.ac_state] <= 0) {
        pause_screen(true, TIMEOUT, true);
    } else {
        pause_screen(false, TIMEOUT, false);
        if (reset) {
            reset_timer(screen_fd, old_val, conf.screen_conf.timeout[state.ac_state]);
        }
    }
}

static void pause_screen(bool pause, enum mod_pause type, bool reset_screen_br) {
    if (CHECK_PAUSE(pause, type)) {
        if (pause) {
            /* Stop capturing snapshots */
            m_deregister_fd(screen_fd);
        } else {
            /* Resume capturing */
            m_register_fd(screen_fd, false, NULL);
        }
    }
    
    /* 
     * Even if we were already paused: 
     * account anyway for new paused request and
     * reset screen br if requested.
     */
    if (pause && reset_screen_br) {
        state.screen_br = 0.0f;
    }
}

static int set_contrib(sd_bus *bus, const char *path, const char *interface, const char *property,
                              sd_bus_message *value, void *userdata, sd_bus_error *error) {
    VALIDATE_PARAMS(value, "d", &contrib_req.contrib.new);
    
    M_PUB(&contrib_req);
    return r;
}
