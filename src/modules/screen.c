#include "bus.h"
#include "my_math.h"

static void get_screen_brightness(bool compute);
static void receive_computing(const msg_t *msg, const void *userdata);
static void timeout_callback(int old_val);
static void dimmed_callback(void);

MODULE("SCREEN");

static double *screen_br;
static int screen_ctr, screen_fd;
static bl_upd screen_msg = { CURRENT_SCR_BL_UPD };

static void init(void) {
    screen_br = calloc(conf.screen_samples, sizeof(double));
    
    M_SUB(CONTRIB_REQ);
    M_SUB(SCR_TO_REQ);
    M_SUB(UPOWER_UPD);
    M_SUB(DISPLAY_UPD);
    
    screen_fd = start_timer(CLOCK_BOOTTIME, 0, 1);
    m_register_fd(screen_fd, false, NULL);
}

static bool check(void) {
    /* Only on X */
    return state.display && state.xauthority;
}

static bool evaluate(void) {
            /* Only when BACKLIGHT and SCREEN modules are enabled */
    return !conf.no_backlight && !conf.no_screen && 
            /* SCREEN configurations have meaningful values */
            conf.screen_contrib > 0 && conf.screen_samples > 0 &&
            /* After UPower */
            state.ac_state != -1;
}

static void destroy(void) {
    free(screen_br);
    if (screen_fd >= 0) {
        close(screen_fd);
    }
}

static void get_screen_brightness(bool compute) {
    SYSBUS_ARG(args, CLIGHTD_SERVICE, "/org/clightd/clightd/Screen", "org.clightd.clightd.Screen", "GetEmittedBrightness");
    call(&screen_br[screen_ctr], "d", &args, "ss", state.display, state.xauthority);
    
    screen_ctr = (screen_ctr + 1) % conf.screen_samples;
    
    if (compute) {
        state.screen_comp = compute_average(screen_br, conf.screen_samples) * conf.screen_contrib;
        screen_msg.curr = state.screen_comp;
        M_PUB(&screen_msg);
        DEBUG("Average screen-emitted brightness: %lf.\n", state.screen_comp);
    } else if (screen_ctr + 1 == conf.screen_samples) {
        /* Bucket filled! Start computing! */
        DEBUG("Start compensating for screen-emitted brightness.\n");
        m_become(computing);
    }
    set_timeout(conf.screen_timeout[state.ac_state], 0, screen_fd, 0);
}

static void receive(const msg_t *msg, const void *userdata) {
    if (!msg->is_pubsub) {
        read_timer(msg->fd_msg->fd);
        get_screen_brightness(false);
    } else if (msg->ps_msg->type == USER) {
        MSG_TYPE();
        switch (type) {
        case SCR_TO_REQ: {
            timeout_upd *up = (timeout_upd *)msg->ps_msg->message;
            conf.screen_timeout[up->state] = up->new;
            if (up->state == state.ac_state) {
                timeout_callback(up->old);
            }
            }
            break;
        case UPOWER_UPD: {
            upower_upd *up = (upower_upd *)msg->ps_msg->message;
            timeout_callback(conf.screen_timeout[up->old]);
            }
            break;
        case DISPLAY_UPD:
            dimmed_callback();
            break;
        case CONTRIB_REQ: {
            contrib_upd *up = (contrib_upd *)msg->ps_msg->message;
            conf.screen_contrib = up->new;
            }
            break;
        default:
            break;
        }
    }
}

static void receive_computing(const msg_t *msg, const void *userdata) {
    if (!msg->is_pubsub) {
        read_timer(msg->fd_msg->fd);
        get_screen_brightness(true);
    } else if (msg->ps_msg->type == USER) {
        MSG_TYPE();
        switch (type) {
        case SCR_TO_REQ: {
            timeout_upd *up = (timeout_upd *)msg->ps_msg->message;
            conf.screen_timeout[up->state] = up->new;
            if (up->state == state.ac_state) {
                timeout_callback(up->old);
            }
            }
            break;
        case UPOWER_UPD: {
            upower_upd *up = (upower_upd *)msg->ps_msg->message;
            timeout_callback(conf.screen_timeout[up->old]);
            }
            break;
        case CONTRIB_REQ: {
            contrib_upd *up = (contrib_upd *)msg->ps_msg->message;
            conf.screen_contrib = up->new;
            /* Recompute current screen compensation */
            state.screen_comp = compute_average(screen_br, conf.screen_samples) * conf.screen_contrib;
            }
            break;
        case DISPLAY_UPD:
            dimmed_callback();
            break;
        default:
            break;
        }
    }
}

static void timeout_callback(int old_val) {
    reset_timer(screen_fd, old_val, conf.screen_timeout[state.ac_state]);
}

static void dimmed_callback(void) {
    if (state.display_state) {
        /* Stop capturing snapshots while dimmed or dpms */
        m_deregister_fd(screen_fd);
    } else {
        /* Resume capturing */
        m_register_fd(screen_fd, false, NULL);
    }
}
