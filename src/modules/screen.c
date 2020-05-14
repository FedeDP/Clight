#include "bus.h"
#include "my_math.h"

enum screen_pause { UNPAUSED = 0, DISPLAY = 0x01, SENSOR = 0x02, LID = 0x04, CONTRIB = 0x08 };

static void receive_waiting_acstate(const msg_t *msg, UNUSED const void *userdata);
static int parse_bus_reply(sd_bus_message *reply, const char *member, void *userdata);
static void get_screen_brightness(bool compute);
static void receive_computing(const msg_t *msg, const void *userdata);
static void timeout_callback(int old_val, bool is_computing);
static void pause_screen(bool pause, enum screen_pause type);

MODULE("SCREEN");

static double *screen_br;
static int screen_ctr, screen_fd = -1;
static int paused_state;

DECLARE_MSG(screen_msg, SCR_BL_UPD);

static void init(void) {
    screen_br = calloc(conf.screen_conf.samples, sizeof(double));
    if (screen_br) {
        M_SUB(CONTRIB_REQ);
        M_SUB(SCR_TO_REQ);
        M_SUB(UPOWER_UPD);
        M_SUB(DISPLAY_UPD);
        M_SUB(SENS_UPD);
        M_SUB(LID_UPD);
    
        m_become(waiting_acstate);
    } else {
        WARN("Failed to init.\n");
        m_poisonpill(self());
    }
}

static bool check(void) {
    /* Only on X */
    return state.display && state.xauthority;
}

static bool evaluate(void) {
    return !conf.screen_conf.disabled && 
            /* SCREEN configurations have meaningful values */
            conf.screen_conf.contrib > 0 && conf.screen_conf.samples > 0;
}

static void destroy(void) {
    free(screen_br);
    if (screen_fd >= 0) {
        close(screen_fd);
    }
}

static void receive_waiting_acstate(const msg_t *msg, UNUSED const void *userdata) {
    switch (MSG_TYPE()) {
    case UPOWER_UPD: {
        /* Start paused if screen timeout for current ac state is <= 0 */
        screen_fd = start_timer(CLOCK_BOOTTIME, 0, conf.screen_conf.timeout[state.ac_state] > 0);
        m_register_fd(screen_fd, false, NULL);
        m_unbecome();
        break;
    }
    default:
        break;
    }
}

static void receive(const msg_t *msg, UNUSED const void *userdata) {
    switch (MSG_TYPE()) {
    case FD_UPD:
        read_timer(msg->fd_msg->fd);
        get_screen_brightness(false);
        break;
    case UPOWER_UPD: {
        upower_upd *up = (upower_upd *)MSG_DATA();
        timeout_callback(conf.screen_conf.timeout[up->old], false);
        break;
    }
    case DISPLAY_UPD:
        pause_screen(state.display_state, DISPLAY);
        break;
    case SENS_UPD:
        pause_screen(!state.sens_avail, SENSOR);
        break;
    case LID_UPD:
        if (conf.bl_conf.pause_on_lid_closed) {
            pause_screen(state.lid_state, LID);
        }
        break;
    case SCR_TO_REQ: {
        timeout_upd *up = (timeout_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            const int old = conf.screen_conf.timeout[up->state];
            conf.screen_conf.timeout[up->state] = up->new;
            if (up->state == state.ac_state) {
                timeout_callback(old, false);
            }
        }
        break;
    }
    case CONTRIB_REQ: {
        contrib_upd *up = (contrib_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            conf.screen_conf.contrib = up->new;
        }
        break;
    }
    default:
        break;
    }
}

static void receive_computing(const msg_t *msg, UNUSED const void *userdata) {
    switch (MSG_TYPE()) {
    case FD_UPD:
        read_timer(msg->fd_msg->fd);
        get_screen_brightness(true);
        break;
    case UPOWER_UPD: {
        upower_upd *up = (upower_upd *)MSG_DATA();
        timeout_callback(conf.screen_conf.timeout[up->old], true);
        break;  
    }
    case DISPLAY_UPD:
        pause_screen(state.display_state, DISPLAY);
        break;
    case SENS_UPD:
        pause_screen(!state.sens_avail, SENSOR);
        break;
    case LID_UPD:
        if (conf.bl_conf.pause_on_lid_closed) {
            pause_screen(state.lid_state, LID);
        }
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
    case CONTRIB_REQ: {
        contrib_upd *up = (contrib_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            const double old = conf.screen_conf.contrib;
            conf.screen_conf.contrib = up->new;
            /* Recompute current screen compensation */
            screen_msg.bl.old = state.screen_comp;
            state.screen_comp = compute_average(screen_br, conf.screen_conf.samples) * conf.screen_conf.contrib;
            if (screen_msg.bl.old != state.screen_comp) {
                screen_msg.bl.new = state.screen_comp;
                M_PUB(&screen_msg);
            }
            /* If screen_comp is now 0, or old screen_comp was 0, check if we need to pause */
            if (up->new == 0 || old == 0) {
                pause_screen(up->new == 0, CONTRIB);
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
        r = sd_bus_message_read(reply, "d", &screen_br[screen_ctr]);
    }
    return r;
}

static void get_screen_brightness(bool compute) {
    SYSBUS_ARG_REPLY(args, parse_bus_reply, NULL, CLIGHTD_SERVICE, "/org/clightd/clightd/Screen", "org.clightd.clightd.Screen", "GetEmittedBrightness");
    
    if (call(&args, "ss", state.display, state.xauthority) == 0) {
        screen_ctr = (screen_ctr + 1) % conf.screen_conf.samples;
        
        if (compute) {
            screen_msg.bl.old = state.screen_comp;
            state.screen_comp = compute_average(screen_br, conf.screen_conf.samples) * conf.screen_conf.contrib;
            if (screen_msg.bl.old != state.screen_comp) {
                screen_msg.bl.new = state.screen_comp;
                M_PUB(&screen_msg);
            }
            DEBUG("Average screen-emitted brightness: %lf.\n", state.screen_comp);
        } else if (screen_ctr + 1 == conf.screen_conf.samples) {
            /* Bucket filled! Start computing! */
            DEBUG("Start compensating for screen-emitted brightness.\n");
            m_become(computing);
        }
    }
    set_timeout(conf.screen_conf.timeout[state.ac_state], 0, screen_fd, 0);
}

static void timeout_callback(int old_val, bool is_computing) {
    reset_timer(screen_fd, old_val, conf.screen_conf.timeout[state.ac_state]);
    /* 
     * A paused timeout has been set; this means user does not want 
     * SCREEN to work in current AC state.
     * Avoid keeping alive old state.screen_comp that won't be never updated,
     * and reset all screen_br values.
     */
    if (conf.screen_conf.timeout[state.ac_state] <= 0) {
        state.screen_comp = 0.0;
        memset(screen_br, 0, conf.screen_conf.samples * sizeof(double));
        screen_ctr = 0;
        
        if (is_computing) {
            m_unbecome();
        }
    }
}

static void pause_screen(bool pause, enum screen_pause type) {
    if (pause) {
        if (paused_state == UNPAUSED) {
            /* Stop capturing snapshots */
            m_deregister_fd(screen_fd);
        }
        paused_state |= type;
    } else {
        paused_state &= ~type;
        if (paused_state == UNPAUSED) {
            /* Resume capturing */
            m_register_fd(screen_fd, false, NULL);
        }
    }
}
