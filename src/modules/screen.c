#include "interface.h"
#include "my_math.h"
#include "utils.h"

static void receive_waiting_acstate(const msg_t *msg, UNUSED const void *userdata);
static int parse_bus_reply(sd_bus_message *reply, const char *member, void *userdata);
static void compute_screen_brightness(void);
static void get_screen_brightness(bool compute);
static void on_contrib_req(bool compute, int new);
static void timeout_callback(int old_val);
static void pause_screen(bool pause, enum mod_pause type);
static int set_screen_contrib(sd_bus *bus, const char *path, const char *interface, const char *property,
                              sd_bus_message *value, void *userdata, sd_bus_error *error);

static bool computing;
static double *screen_br;
static int screen_ctr, screen_fd = -1;
static const sd_bus_vtable conf_screen_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("NumSamples", "i", NULL, offsetof(screen_conf_t, samples), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_WRITABLE_PROPERTY("Contrib", "d", NULL, set_screen_contrib, offsetof(screen_conf_t, contrib), 0),
    SD_BUS_WRITABLE_PROPERTY("AcTimeout", "i", NULL, set_timeouts, offsetof(screen_conf_t, timeout[ON_AC]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattTimeout", "i", NULL, set_timeouts, offsetof(screen_conf_t, timeout[ON_BATTERY]), 0),
    SD_BUS_VTABLE_END
};

DECLARE_MSG(screen_msg, SCR_BL_UPD);
DECLARE_MSG(contrib_req, CONTRIB_REQ);

API(Screen, conf_screen_vtable, conf.screen_conf);
MODULE_WITH_PAUSE("SCREEN");

static void init(void) {
    screen_br = calloc(conf.screen_conf.samples, sizeof(double));
    if (screen_br) {
        M_SUB(CONTRIB_REQ);
        M_SUB(SCR_TO_REQ);
        M_SUB(UPOWER_UPD);
        M_SUB(DISPLAY_UPD);
        M_SUB(SENS_UPD);
        M_SUB(LID_UPD);
        M_SUB(SUSPEND_UPD);
        
        m_become(waiting_acstate);
        
        init_Screen_api();
    } else {
        WARN("Failed to init. Killing module.\n");
        module_deregister((self_t **)&self());
    }
}

static bool check(void) {
    return true;
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
    deinit_Screen_api();
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
        get_screen_brightness(computing);
        break;
    case UPOWER_UPD: {
        upower_upd *up = (upower_upd *)MSG_DATA();
        timeout_callback(conf.screen_conf.timeout[up->old]);
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
    case SUSPEND_UPD:
         pause_screen(state.suspended, SUSPEND);
         break;
    case SCR_TO_REQ: {
        timeout_upd *up = (timeout_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            const int old = conf.screen_conf.timeout[up->state];
            conf.screen_conf.timeout[up->state] = up->new;
            if (up->state == state.ac_state) {
                timeout_callback(old);
            }
        }
        break;
    }
    case CONTRIB_REQ: {
        contrib_upd *up = (contrib_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            on_contrib_req(computing, up->new);
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

static void compute_screen_brightness(void) {
    const double old_comp = state.screen_comp;
    state.screen_comp = compute_average(screen_br, conf.screen_conf.samples) * conf.screen_conf.contrib;
    if (old_comp != state.screen_comp) {
        screen_msg.bl.old = old_comp;
        screen_msg.bl.new = state.screen_comp;
        M_PUB(&screen_msg);
    }
    DEBUG("Average screen-emitted brightness: %lf.\n", state.screen_comp);
}

static void get_screen_brightness(bool compute) {
    SYSBUS_ARG_REPLY(args, parse_bus_reply, NULL, CLIGHTD_SERVICE, "/org/clightd/clightd/Screen", "org.clightd.clightd.Screen", "GetEmittedBrightness");
    
    if (call(&args, "ss", fetch_display(), fetch_env()) == 0) {
        screen_ctr = (screen_ctr + 1) % conf.screen_conf.samples;
        
        if (compute) {
            compute_screen_brightness();
        } else if (screen_ctr + 1 == conf.screen_conf.samples) {
            /* Bucket filled! Start computing! */
            DEBUG("Start compensating for screen-emitted brightness.\n");
            computing = true;
        }
    }
    set_timeout(conf.screen_conf.timeout[state.ac_state], 0, screen_fd, 0);
}

static void on_contrib_req(bool compute, int new) {
    const double old = conf.screen_conf.contrib;
    conf.screen_conf.contrib = new;
    if (compute) {
        /* Recompute current screen compensation */
        compute_screen_brightness();
    }
    /* If screen_comp is now 0, or old screen_comp was 0, check if we need to pause/resume */
    if (new == 0 || old == 0) {
        pause_screen(new == 0, CONTRIB);
    }
}

static void timeout_callback(int old_val) {
    if (conf.screen_conf.timeout[state.ac_state] <= 0) {
        pause_screen(true, TIMEOUT);
    } else {
        pause_screen(false, TIMEOUT);
        reset_timer(screen_fd, old_val, conf.screen_conf.timeout[state.ac_state]);
    }
}

static void pause_screen(bool pause, enum mod_pause type) {
    if (CHECK_PAUSE(pause, type, "SCREEN")) {
        if (pause) {
            /* Stop capturing snapshots */
            m_deregister_fd(screen_fd);
        } else {
            /* Resume capturing */
            m_register_fd(screen_fd, false, NULL);
        }
    }
}

static int set_screen_contrib(sd_bus *bus, const char *path, const char *interface, const char *property,
                       sd_bus_message *value, void *userdata, sd_bus_error *error) {
    VALIDATE_PARAMS(value, "d", &contrib_req.contrib.new);
    
    M_PUB(&contrib_req);
    return r;
}
