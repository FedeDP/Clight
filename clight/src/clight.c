#define _GNU_SOURCE

#include <poll.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <signal.h>
#include <sys/timerfd.h>
#include <xcb/dpms.h>
#include <popt.h>

#include "../inc/bus.h"
#include "../inc/brightness.h"
#include "../inc/gamma.h"
#include "../inc/location.h"

#define CAPTURE_IX 0
#define SIGNAL_IX 1
#define GAMMA_IX 2
#define LOCATION_IX 3

#define SMOOTH_TRANSITION_TIMEOUT 300 * 1000 * 1000

static void init_config(int argc, char *argv[]);
static void setup_everything(void);
static void parse_cmd(int argc, char * const argv[]);
static void init_dpms(void);
static int set_signals(void);
static void set_pollfd(void);
static void free_everything(void);
static int start_timer(int clockid, int initial_timeout);
static void set_timeout(int sec, int nsec, int fd, int flag);
static void sig_handler(int fd);
static void do_capture(void);
static int get_screen_dpms(void);
static void check_gamma(void);
static void on_new_location(void);
static void main_poll(void);

static const int nfds = 4;

static int dpms_enabled, single_capture_mode;
static struct pollfd *main_p;
static xcb_connection_t *connection;

int main(int argc, char *argv[]) {
    open_log();
    init_config(argc, argv);
    setup_everything();
    main_poll();
    free_everything();
    close_log();
    return 0;
}

/*
 * Init default config values and parse cmdline args through popt lib.
 */
static void init_config(int argc, char *argv[]) {
    /* default values */
    conf.num_captures = 5;
    conf.timeout[DAY] = 10 * 60; // 10 mins during the day
    conf.timeout[NIGHT] = 45 * 60; // 45 mins during the night
    conf.timeout[EVENT] = 3 * 60; // 3 mins during an event
    conf.timeout[UNKNOWN] = conf.timeout[DAY]; // if unknown, fallback to 10mins
    conf.temp[DAY] = 6500;
    conf.temp[NIGHT] = 4000;
    conf.temp[EVENT] = -1;
    conf.temp[UNKNOWN] = -1;
    conf.smooth_transition = 1;

    parse_cmd(argc, argv);

    /*
     * Reset default values in case of wrong values
     */
    if (conf.timeout[DAY] <= 0) {
        ERROR("Wrong day timeout value. Resetting default value.\n");
        conf.timeout[DAY] = 10 * 60;
    }

    if (conf.timeout[NIGHT] <= 0) {
        ERROR("Wrong night timeout value. Resetting default value.\n");
        conf.timeout[NIGHT] = 45 * 60;
    }

    if (conf.num_captures <= 0) {
        ERROR("Wrong frames value. Resetting default value.\n");
        conf.num_captures = 5;
    }

    if (conf.temp[DAY] < 1000 || conf.temp[DAY] > 10000) {
        ERROR("Wrong daily temp value. Resetting default value.\n");
        conf.temp[DAY] = 6500;
    }

    if (conf.temp[NIGHT] < 1000 || conf.temp[NIGHT] > 10000) {
        ERROR("Wrong nightly temp value. Resetting default value.\n");
        conf.temp[NIGHT] = 4000;
    }

    log_conf();
}

/*
 * Create every needed struct/variable.
 */
static void setup_everything(void) {
    if (!state.quit) {
        init_bus();
        if (!state.quit) {
            init_brightness();
            if (!state.quit) {
                if (single_capture_mode) {
                    do_capture();
                    state.quit = 1;
                } else {
                    init_dpms();
                    set_pollfd();
                }
            }
        }
    }
}

/**
 * Parse cmdline to get cmd line options
 */
static void parse_cmd(int argc, char *const argv[]) {
    poptContext pc;
    struct poptOption po[] = {
        {"capture", 'c', POPT_ARG_NONE, &single_capture_mode, 0, "Take a fast capture/screen brightness calibration and quit.", NULL},
        {"frames", 'f', POPT_ARG_INT, &conf.num_captures, 0, "Frames taken for each capture. Defaults to 5.", "Number of frames to be taken."},
        {"day_timeout", 0, POPT_ARG_INT, &conf.timeout[DAY], 0, "Timeout between captures during the day. Defaults to 10mins.", "Number of seconds between each capture."},
        {"night_timeout", 0, POPT_ARG_INT, &conf.timeout[NIGHT], 0, "Timeout between captures during the night. Defaults to 45mins.", "Number of seconds between each capture."},
        {"device", 'd', POPT_ARG_STRING, NULL, 1, "Path to webcam device. By default, first matching device is used.", "Webcam device to be used."},
        {"backlight", 'b', POPT_ARG_STRING, NULL, 2, "Path to backlight syspath. By default, first matching device is used.", "Backlight to be used."},
        {"smooth_transition", 0, POPT_ARG_INT, &conf.smooth_transition, 0, "Whether to enable smooth gamma transition.", "1 enable/0 disable."},
        {"day_temp", 0, POPT_ARG_INT, &conf.temp[DAY], 0, "Daily gamma temperature.", "Between 1000 and 10000."},
        {"night_temp", 0, POPT_ARG_INT, &conf.temp[NIGHT], 0, "Nightly gamma temperature.", "Between 1000 and 10000."},
        {"lat", 0, POPT_ARG_DOUBLE, &conf.lat, 0, "Your current latitude.", NULL},
        {"lon", 0, POPT_ARG_DOUBLE, &conf.lon, 0, "Your current longitude.", NULL},
        POPT_AUTOHELP
        {NULL}
    };

    pc = poptGetContext(NULL, argc, (const char **)argv, po, 0);
    // process options and handle each val returned
    int rc;
    while ((rc = poptGetNextOpt(pc)) >= 0) {
        switch (rc) {
        case 1:
            strncpy(conf.dev_name, poptGetOptArg(pc), PATH_MAX);
            break;
        case 2:
            strncpy(conf.screen_path, poptGetOptArg(pc), PATH_MAX);
            break;
        }
    }
    // poptGetNextOpt returns -1 when the final argument has been parsed
    // otherwise an error occured
    if (rc != -1) {
        ERROR("%s\n", poptStrerror(rc));
        poptFreeContext(pc);
        exit(1);
    }
    poptFreeContext(pc);
}

/**
 * Checks through xcb if DPMS is enabled for this xscreen
 */
static void init_dpms(void) {
    connection = xcb_connect(NULL, NULL);

    if (!xcb_connection_has_error(connection)) {
        xcb_dpms_info_cookie_t cookie;
        xcb_dpms_info_reply_t *info;

        cookie = xcb_dpms_info(connection);
        info = xcb_dpms_info_reply(connection, cookie, NULL);

        if (info->state) {
            dpms_enabled = 1;
        }

        free(info);
    }
}

/**
 * Set signals handler for SIGINT and SIGTERM (using a signalfd)
 */
static int set_signals(void) {
    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    return signalfd(-1, &mask, 0);
}

/**
 * Create the pollfd struct
 */
static void set_pollfd(void) {
    if (!(main_p = calloc(nfds, sizeof(struct pollfd)))) {
        state.quit = 1;
        return;
    }
    // init timerfd for captures, with initial timeout of 1s
    int capture_timerfd = start_timer(CLOCK_MONOTONIC, 1);
    // init location receiver fd
    int location_fd = init_location();
    // init gamma timer disarmed: seconds = 0
    int gamma_timerfd = start_timer(CLOCK_REALTIME, 0);
    if (state.quit) {
        ERROR("%s\n", strerror(errno));
        return;
    }

    main_p[GAMMA_IX] = (struct pollfd) {
        .fd = gamma_timerfd,
        .events = POLLIN,
    };
    main_p[LOCATION_IX] = (struct pollfd) {
        .fd = location_fd,
        .events = POLLIN,
    };
    main_p[CAPTURE_IX] = (struct pollfd) {
        .fd = capture_timerfd,
        .events = POLLIN,
    };
    main_p[SIGNAL_IX] = (struct pollfd) {
        .fd = set_signals(),
        .events = POLLIN,
    };
}

/**
 * Free every used resource
 */
static void free_everything(void) {
    if (main_p) {
        // nfds - geoclue -> because we should not close last fd if it points to a sd_bus_fd
        for (int i = 0; i < nfds - is_geoclue(); i++) {
            if (main_p[i].fd != -1) {
                close(main_p[i].fd);
            }
        }
        free(main_p);
    }

    destroy_geoclue();
    destroy_bus();
    free_brightness();

    if (connection) {
        xcb_disconnect(connection);
    }
}

/**
 * Create timer and returns its fd to
 * the main struct pollfd
 */
static int start_timer(int clockid, int initial_timeout) {
    int timerfd = timerfd_create(clockid, 0);
    if (timerfd == -1) {
        ERROR("could not start timer: %s\n", strerror(errno));
        state.quit = 1;
    } else {
        set_timeout(initial_timeout, 0, timerfd, 0);
    }
    return timerfd;
}

/**
 * Helper to set a new trigger on timerfd in $start seconds
 */
static void set_timeout(int sec, int nsec, int fd, int flag) {
    struct itimerspec timerValue = {{0}};

    timerValue.it_value.tv_sec = sec;
    timerValue.it_value.tv_nsec = nsec;
    timerValue.it_interval.tv_sec = 0;
    timerValue.it_interval.tv_nsec = 0;
    int r = timerfd_settime(fd, flag, &timerValue, NULL);
    if (r == -1) {
        ERROR("%s\n", strerror(errno));
        state.quit = 1;
    }
}

/*
 * if received an external SIGINT or SIGTERM,
 * just switch the quit flag to 1 and print to stdout.
 */
static void sig_handler(int fd) {
    struct signalfd_siginfo fdsi;
    ssize_t s;

    s = read(fd, &fdsi, sizeof(struct signalfd_siginfo));
    if (s != sizeof(struct signalfd_siginfo)) {
        ERROR("an error occurred while getting signalfd data.\n");
    } else {
        INFO("received signal %d. Leaving.\n", fdsi.ssi_signo);
    }
    state.quit = 1;
}

/**
 * When timerfd timeout expires, check if we are in screen power_save mode,
 * otherwise start streaming on webcam and set CAMERA_IX fd of pollfd struct to
 * webcam device fd. This way our main poll will get events (frames) from webcam device too.
 */
static void do_capture(void) {
    static const int fast_timeout = 15;
    static const double drop_limit = 0.6;

    /*
     * if screen is currently blanked thanks to dpms,
     * do not do anything. Set a long timeout and return.
     * Timeout will increase as screen power management goes deeper.
     */
    if (dpms_enabled && get_screen_dpms() > 0) {
        INFO("Screen is currently in power saving mode. Avoid changing brightness and setting a long timeout.\n");
        return set_timeout(2 * conf.timeout[state.time] * get_screen_dpms(), 0, main_p[CAPTURE_IX].fd, 0);
    }

    double drop = 0.0;

    for (int i = 0; i < conf.num_captures && !state.quit; i++) {
        state.values[i] = capture_frame();
    }

    if (!state.quit) {
        double new_val = compute_avg_brightness();
        if (new_val != 0.0) {
            INFO("Average frames brightness: %lf.\n", new_val);
            drop = set_brightness(new_val);
        }
    }

    if (!single_capture_mode && !state.quit) {
        // if there is too high difference, do a fast recapture to be sure
        // this is the correct level
        if (fabs(drop) > drop_limit) {
            INFO("Weird brightness drop. Recapturing in 15 seconds.\n");
            // single call after 15s
            set_timeout(fast_timeout, 0, main_p[CAPTURE_IX].fd, 0);
        } else {
            // reset normal timer
            set_timeout(conf.timeout[state.time], 0, main_p[CAPTURE_IX].fd, 0);
        }
    }
}

/**
 * info->power_level is one of:
 * DPMS Extension Power Levels
 * 0     DPMSModeOn          In use
 * 1     DPMSModeStandby     Blanked, low power
 * 2     DPMSModeSuspend     Blanked, lower power
 * 3     DPMSModeOff         Shut off, awaiting activity
 */
static int get_screen_dpms(void) {
    xcb_dpms_info_cookie_t cookie;
    xcb_dpms_info_reply_t *info;
    int ret = -1;

    cookie = xcb_dpms_info(connection);
    info = xcb_dpms_info_reply(connection, cookie, NULL);

    if (info) {
        ret = info->power_level;
        free(info);
    }
    return ret;
}

/*
 * check current period of time (day or night) and next event (sunrise/sunset).
 * Then call set_temp with correct temp, given current state(night or day).
 * It returns 0 if: smooth_transition is off or desired temp has finally been setted (after transitioning).
 * If it returns 0, reset transitioning flag and set next event timeout.
 * Else, set a timeout for smooth transition and set transitioning flag to 1.
 */
static void check_gamma(void) {
    static int transitioning = 0;

    if (!transitioning) {
        get_next_gamma_event(conf.lat, conf.lon);
    }

    int ret = set_temp(conf.temp[state.time]);
    if (state.quit) {
        return;
    }

    if (ret == 0) {
        INFO("Next gamma alarm due to: %s", ctime(&(state.next_event)));
        set_timeout(state.next_event, 0, main_p[GAMMA_IX].fd, TFD_TIMER_ABSTIME);
        transitioning = 0;
    } else {
        // here, set a timeout of 300ms from now for smooth gamma transition
        set_timeout(0, SMOOTH_TRANSITION_TIMEOUT, main_p[GAMMA_IX].fd, 0);
        transitioning = 1;
    }
}

/*
 * When a new location is received, reset GAMMA timer to now + 1sec;
 * this way, check_gamma will be called and it will correctly set new timer.
 */
static void on_new_location(void) {
    INFO("New location received: %.2lf, %.2lf\n", conf.lat, conf.lon);
    set_timeout(1, 0, main_p[GAMMA_IX].fd, 0);
}

static void main_poll(void) {
    uint64_t t;

    while (!state.quit) {
        int r = poll(main_p, nfds, -1);
        if (r == -1) {
            if (errno == EINTR) {
                continue;
            }
            state.quit = 1;
            return;
        }

        for (int i = 0; i < nfds && r > 0; i++) {
            if (main_p[i].revents & POLLIN) {
                switch (i) {
                case CAPTURE_IX:
                    /* we received a timer expiration signal on timerfd */
                    read(main_p[i].fd, &t, sizeof(uint64_t));
                    do_capture();
                    break;
                case SIGNAL_IX:
                    /* we received a signal */
                    sig_handler(main_p[i].fd);
                    break;
                case GAMMA_IX:
                    /* we received a timer expiration for gamma */
                    read(main_p[i].fd, &t, sizeof(uint64_t));
                    check_gamma();
                    break;
                case LOCATION_IX:
                    /* we received a new user position */
                    if (!is_geoclue()) {
                        /* it is not from a bus signal as geoclue2 is not being used */
                        read(main_p[i].fd, &t, sizeof(uint64_t));
                    } else {
                        sd_bus_process(bus, NULL);
                    }
                    on_new_location();
                    break;
                }
                r--;
            }
        }
    }
}
