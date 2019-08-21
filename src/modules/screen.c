#include "bus.h"
#include "my_math.h"

static void receive_computing(const msg_t *msg, const void *userdata);
static void timeout_callback(int fd, int old_val);
static void dimmed_callback(int fd);

MODULE("SCREEN");

const char *current_scr_topic = "CurrentScreenComp";
static double *screen_br;
static int screen_ctr;
static bl_upd screen_msg = { CURRENT_SCR_BL };

static void init(void) {
    screen_br = calloc(conf.screen_samples, sizeof(double));
    
    m_subscribe(interface_scr_contrib);
    m_subscribe(interface_scr_to_topic);
    m_subscribe(up_topic);
    m_subscribe(display_topic);
    
    int fd = start_timer(CLOCK_BOOTTIME, 0, 1);
    m_register_fd(fd, true, NULL);
}

static bool check(void) {
    /* Only on X */
    return state.display != NULL;
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
}

static void receive(const msg_t *msg, const void *userdata) {
    if (!msg->is_pubsub) {
        read_timer(msg->fd_msg->fd);
        
        SYSBUS_ARG(args, CLIGHTD_SERVICE, "/org/clightd/clightd/Screen", "org.clightd.clightd.Screen", "GetEmittedBrightness");
        call(&screen_br[screen_ctr], "d", &args, "ss", state.display, state.xauthority);
        
        screen_ctr = (screen_ctr + 1) % conf.screen_samples;
        if (screen_ctr + 1 == conf.screen_samples) {
            /* Bucket filled! Start computing! */
            DEBUG("Start compensating for screen-emitted brightness.\n");
            m_become(computing);
        }
        set_timeout(conf.screen_timeout[state.ac_state], 0, msg->fd_msg->fd, 0);
    } else if (msg->ps_msg->type == USER) {
        MSG_TYPE();
        switch (type) {
        case TIMEOUT_UPD: {
            timeout_upd *up = (timeout_upd *)msg->ps_msg->message;
            timeout_callback(msg->fd_msg->fd, up->old);
            }
            break;
        case UPOWER_UPD: {
            upower_upd *up = (upower_upd *)msg->ps_msg->message;
            timeout_callback(msg->fd_msg->fd, conf.screen_timeout[up->old]);
            }
            break;
        case DISPLAY_UPD:
            dimmed_callback(msg->fd_msg->fd);
            break;
        default:
            break;
        }
    }
}

static void receive_computing(const msg_t *msg, const void *userdata) {
    if (!msg->is_pubsub) {
        read_timer(msg->fd_msg->fd);
        
        SYSBUS_ARG(args, CLIGHTD_SERVICE, "/org/clightd/clightd/Screen", "org.clightd.clightd.Screen", "GetEmittedBrightness");
        call(&screen_br[screen_ctr], "d", &args, "ss", state.display, state.xauthority);
        
        /* Circular array */
        screen_ctr = (screen_ctr + 1) % conf.screen_samples;
        state.screen_comp = compute_average(screen_br, conf.screen_samples) * conf.screen_contrib;
        screen_msg.curr = state.screen_comp;
        M_PUB(current_scr_topic, &screen_msg);
        DEBUG("Average screen-emitted brightness: %lf.\n", state.screen_comp);
        set_timeout(conf.screen_timeout[state.ac_state], 0, msg->fd_msg->fd, 0);
    } else if (msg->ps_msg->type == USER) {
        MSG_TYPE();
        switch (type) {
        case TIMEOUT_UPD: {
            timeout_upd *up = (timeout_upd *)msg->ps_msg->message;
            timeout_callback(msg->fd_msg->fd, up->old);
            }
            break;
        case UPOWER_UPD: {
            upower_upd *up = (upower_upd *)msg->ps_msg->message;
            timeout_callback(msg->fd_msg->fd, conf.screen_timeout[up->old]);
            }
            break;
        case CONTRIB_UPD:
            /* Recompute current screen compensation */
            state.screen_comp = compute_average(screen_br, conf.screen_samples) * conf.screen_contrib;
            break;
        case DISPLAY_UPD:
            dimmed_callback(msg->fd_msg->fd);
            break;
        default:
            break;
        }
    }
}

static void timeout_callback(int fd, int old_val) {
    reset_timer(fd, old_val, conf.screen_timeout[state.ac_state]);
}

static void dimmed_callback(int fd) {
    if (state.display_state) {
        /* Stop capturing snapshots while dimmed or dpms */
        set_timeout(0, 0, fd, 0);
    } else {
        /* Resume capturing */
        set_timeout(conf.screen_timeout[state.ac_state], 0, fd, 0);
    }
}
