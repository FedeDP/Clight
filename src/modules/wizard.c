#include "bus.h"
#include "my_math.h"

static void receive_capturing(const msg_t *const msg, UNUSED const void* userdata);
static void receive_calibrating(const msg_t *const msg, UNUSED const void* userdata);
static char read_char(int fd);
static void next_step(void);
static int parse_bus_reply(sd_bus_message *reply, const char *member, void *userdata);
static int get_backlight(void);
static void compute(void);
static void expand_regr_points(void);

#define WIZ_IN_POINTS 5
#define WIZ_OUT_POINTS 11

DECLARE_MSG(capture_req, CAPTURE_REQ);

MODULE("WIZARD");

typedef enum {
    WIZ_START,
    WIZ_CAPTURE_NIGHTLIGHT,
    WIZ_CALIB_NIGHTLIGHT,
    WIZ_CAPTURE_DARK_ROOM,
    WIZ_CALIB_DARK_ROOM,
    WIZ_CAPTURE_NORMAL_ROOM,
    WIZ_CALIB_NORMAL_ROOM,
    WIZ_CAPTURE_BRIGHT_ROOM,
    WIZ_CALIB_BRIGHT_ROOM,
    WIZ_CAPTURE_DAYLIGHT,
    WIZ_CALIB_DAYLIGHT,
    WIZ_COMPUTE
} wiz_states;

static wiz_states wiz_st;
static double bls[WIZ_IN_POINTS];
static double amb_brs[WIZ_IN_POINTS];
static double output[DEGREE];
static int capture_idx;

static void init(void) {
    setbuf(stdout, NULL); // disable line buffer
    capture_req.capture.reset_timer = false;
    capture_req.capture.capture_only = true;

    M_SUB(AMBIENT_BR_UPD);
    m_register_fd(STDIN_FILENO, false, NULL);
    INFO("Welcome to Clight wizard.\n");
    if (!state.sens_avail) {
        INFO("No sensors available. Plug it in and restart wizard.\n");
        modules_quit(EXIT_FAILURE);
    } else {
        INFO("Press ctrl-c to quit at any moment, discarding changes.\n");
        INFO("\nStart? [Y/n]: > ");
    }
}

static bool check(void) {
    return true;
}

static bool evaluate() {
    return conf.wizard;
}

static void receive(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
    case FD_UPD: {
        char c = read_char(msg->fd_msg->fd);
        switch (c) {
        case 'y':
        case 'Y':
        case 10:
            INFO("\n");
            next_step();
            break;
        default:
            modules_quit(0);
            break;
        }
        break;
    }
    default:
        break;
    }
}

static void receive_capturing(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
    case FD_UPD: {
        char c = read_char(msg->fd_msg->fd);
        switch (c) {
        case 'y':
        case 'Y':
        case 10:
            if (!amb_brs[capture_idx] || c != 10) {
                M_PUB(&capture_req);
                break;
            }
        default:
            INFO("Set desired screen backlight then press ENTER... > ");
            next_step();
            break;
        }
        break;
    }
    case AMBIENT_BR_UPD:
        amb_brs[capture_idx] = state.ambient_br;
        INFO("Ambient brightness is currently %.3lf; redo capture? [y/N]: > ", state.ambient_br);
        break;
    default:
        break;
    }
}

static void receive_calibrating(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
    case FD_UPD: {
        char c = read_char(msg->fd_msg->fd);
        switch (c) {
        case 10:
            if (get_backlight() != 0) {
                WARN("Failed to get '%s' backlight pct.\n", conf.bl_conf.screen_path);
                modules_quit(-1);
            } else {
                INFO("Backlight level is: %.3lf\n\n", bls[capture_idx - 1]);
                next_step();
            }
            break;
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
}

static void destroy(void) {
    
}

static char read_char(int fd) {
    char c[10] = {0};
    read(fd, c, 9);
    return c[0];
}

static void next_step(void) {
    wiz_st++;
    if (wiz_st < WIZ_COMPUTE) {
        if (wiz_st % 2 == 0) {
            m_become(calibrating);
        } else {
            switch (wiz_st) {
            case WIZ_CAPTURE_DAYLIGHT:
                INFO("Move to daylight-like light.\n");
                break;
            case WIZ_CAPTURE_BRIGHT_ROOM:
                INFO("Move to a bright room.\n");
                break;
            case WIZ_CAPTURE_NORMAL_ROOM:
                INFO("Move to normal-light room.\n");
                break;
            case WIZ_CAPTURE_DARK_ROOM:
                INFO("Move to a dark room.\n");
                break;
            case WIZ_CAPTURE_NIGHTLIGHT:
                INFO("Move to nightlight-like light.\n");
                break;
            default:
                break;
            }
            INFO("Are you ready? [Y/n]\n");
            m_become(capturing);
        }
    } else {
        INFO("Computing new backlight curve...\n");
        compute();
        INFO("Computing new regression points...\n");
        expand_regr_points();
        INFO("Don't forget to set these points in clight conf file!\nBye!\n");
        modules_quit(0);
    }
}

static int parse_bus_reply(sd_bus_message *reply, const char *member, void *userdata) {
    return sd_bus_message_read(reply, "(sd)", NULL, &bls[capture_idx++]);
}

static int get_backlight() {
    SYSBUS_ARG_REPLY(args, parse_bus_reply, NULL, CLIGHTD_SERVICE, "/org/clightd/clightd/Backlight", "org.clightd.clightd.Backlight", "Get");
    return call(&args, "s", conf.bl_conf.screen_path);
}

static void compute(void) {
    for (int i = 0; i < WIZ_IN_POINTS; i++) {
        DEBUG("%.3lf -> %.3lf\n", amb_brs[i], bls[i]);
    }
    polynomialfit(amb_brs, bls, output, WIZ_IN_POINTS);
    DEBUG("New curve: y = %lf + %lfx + %lfx^2\n", output[0], output[1], output[2]);
}

static void expand_regr_points(void) {
    INFO("[ ");
    for (int i = 0; i < WIZ_OUT_POINTS; i++) {
        const double perc = i * (WIZ_IN_POINTS - 1);
        const double b = output[0] + output[1] * perc + output[2] * pow(perc, 2);
        const double new_br_pct =  clamp(b, 1, 0);
        INFO("%.3lf%s ", new_br_pct, i < WIZ_OUT_POINTS - 1 ? ", " : " ");
    }
    INFO("]\n");
}
