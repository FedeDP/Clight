#define _GNU_SOURCE

#include <poll.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <signal.h>
#include <sys/timerfd.h>
#include <math.h>
#include <xcb/dpms.h>
#include <getopt.h>
#include <time.h>

#include "../inc/brightness.h"
#include "../inc/config.h"

#define TIMER_IX 0
#define SIGNAL_IX 1

static const int fast_timeout = 15;
static const int nfds = 2;
static const double drop_limit = 0.6;

static int dpms_enabled, single_capture_mode;
static struct pollfd *main_p;
static xcb_connection_t *connection;

static void init_config(int argc, char *argv[]);
static void setup_everything(void);
static void parse_cmd(int argc, char * const argv[]);
static void print_help(void);
static void init_dpms(void);
static int set_signals(void);
static void set_pollfd(void);
static void free_everything(void);
static int start_timer(void);
static void set_timeout(int start, int fd);
static void sig_handler(int fd);
static void do_capture(void);
static int get_screen_dpms(void);
static void main_poll(void);

int main(int argc, char *argv[]) {
    init_config(argc, argv);
    setup_everything();
    main_poll();
    free_everything();
    return 0;
}

static void init_config(int argc, char *argv[]) {
    // default values
    conf.num_captures = 5;
    conf.timeout = 300;
    
    init_config_file();
    read_config();
    parse_cmd(argc, argv);
    
    /* Reset default values in case of wrong values */
    if (conf.timeout <= 0) {
        conf.timeout = 300;
    }
    
    if (conf.num_captures <= 0) {
        conf.num_captures = 5;
    }
}

static void setup_everything(void) {
    if (!state.quit) {
        init_brightness();
        if (!state.quit) {
            printf("Using %d frames captures with timeout %d.\n", conf.num_captures, conf.timeout);
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

/**
 * Parse cmdline to get cmd line options
 */
static void parse_cmd(int argc, char *const argv[]) {
    int idx = 0, opt;
    
    static struct option opts[] = {
        {"capture", no_argument, 0, 'c'},
        {"frames", required_argument, 0, 'f'},
        {"timeout", required_argument, 0, 't'},
        {"setup", no_argument, 0, 's'},
        {"device", required_argument, 0, 'd'},
        {"backlight", required_argument, 0, 'b'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    while ((opt = getopt_long(argc, argv, "cf:t:sd:b:h", opts, &idx)) != -1) {
        switch (opt) {
        case 'c':
            single_capture_mode = 1;
            printf("Entered fast capture mode.\n");
            break;
        case 'f':
            conf.num_captures =  atoi(optarg);
            break;
        case 't':
            conf.timeout = atoi(optarg);
            break;
        case 's':
            setup_config();
            state.quit = 1;
            break;
        case 'd':
            strncpy(conf.dev_name, optarg, PATH_MAX);
            break;
        case 'b':
            strncpy(conf.screen_path, optarg, PATH_MAX);
            break;
        case 'h':
            print_help();
            state.quit = 1;
            return;
        case '?':
            state.quit = 1;
            return;
        default:
            break;
        }
    }
}

static void print_help(void) {
    printf("\n Clight\n");
    printf("\n Copyright (C) 2016  Federico Di Pierro (https://github.com/FedeDP):\n");
    printf(" This program comes with ABSOLUTELY NO WARRANTY;\n");
    printf(" This is free software, and you are welcome to redistribute it under certain conditions;\n");
    printf(" It is GPL licensed. Have a look at COPYING file.\n\n");
    printf("\tIt supports following cmdline options:\n");
    printf("\t* --frames/-f number_of_frames -> frames taken for each capture. Defaults to 5.\n");
    printf("\t* --timeout/-t number_of_seconds -> timeout between captures. Defaults to 300.\n");
    printf("\t* --device/-d /dev/videoX -> path to webcam device. Defaults to /dev/video0.\n");
    printf("\t* --backlight/-b /sys/class/backlight/... -> path to backlight syspath. Defaults to /sys/class/backlight/intel_backlight.\n");
    printf("\t* --capture/-c -> to take a fast capture/ screen brightness calibration and quit.\n");
    printf("\t* --setup/-s -> to interactively create a config file.\n\n");
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
    main_p = calloc(nfds, sizeof(struct pollfd));
    int timerfd = start_timer();
    if (timerfd == -1 || !main_p) {
        state.quit = 1;
        return;
    }
    main_p[TIMER_IX] = (struct pollfd) {
        .fd = timerfd,
        .events = POLLIN,
    };
    main_p[SIGNAL_IX] = (struct pollfd) {
        .fd = set_signals(),
        .events = POLLIN,
    };
}

/**
 * Free every resource used
 */
static void free_everything(void) {
    if (main_p) {
        for (int i = 0; i < SIGNAL_IX; i++) {
            close(main_p[i].fd);
        }
        free(main_p);
    }
    
    free_brightness();
    
    if (connection) {
        xcb_disconnect(connection);
    }
}

/**
 * Create timer and returns its fd to
 * the main struct pollfd
 */
static int start_timer(void) {
    int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (timerfd == -1) {
        fprintf(stderr, "could not start timer.\n");
        return -1;
    }
    printf("Started timer.\n");
    // first capture immediately, then every 5min
    set_timeout(1, timerfd);
    return timerfd;
}

/**
 * Helper to set a new trigger on timerfd in $start seconds
 */
static void set_timeout(int start, int fd) {
    struct itimerspec timerValue = {{0}};
    
    timerValue.it_value.tv_sec = start;
    timerValue.it_value.tv_nsec = 0;
    timerValue.it_interval.tv_sec = 0;
    timerValue.it_interval.tv_nsec = 0;
    timerfd_settime(fd, 0, &timerValue, NULL);
    
    printf("Timer expiring in: %d seconds.\n", start);
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
        fprintf(stderr, "an error occurred while getting signalfd data.\n");
    } else {
        printf("received signal %d. Leaving.\n", fdsi.ssi_signo);
    }
    state.quit = 1;
}

/**
 * When timerfd timeout expires, check if we are in screen power_save mode,
 * otherwise start streaming on webcam and set CAMERA_IX fd of pollfd struct to
 * webcam device fd. This way our main poll will get events (frames) from webcam device too.
 */
static void do_capture(void) {
    // if screen is currently blanked thanks to dpms,
    // do not do anything. Set a long timeout and return.
    // Timeout will increase as screen power management goes deeper.
    if (dpms_enabled && get_screen_dpms() > 0) {
        printf("Screen is currently in power saving mode. Avoid changing brightness and setting a long timeout.\n");
        set_timeout(2 * conf.timeout * get_screen_dpms(), main_p[TIMER_IX].fd);
        return;
    }
    
    double drop = 0.0;
        
    for (int i = 0; i < conf.num_captures && !state.quit; i++) {
        state.values[i] = capture_frame();
    }
    
    if (!state.quit) {
        double new_val = compute_backlight();
        if (new_val != 0.0) {
            drop = set_brightness(new_val);
        }
    }
    
    if (!single_capture_mode && !state.quit) {
        // if there is too high difference, do a fast recapture to be sure
        // this is the correct level
        if (fabs(drop) > drop_limit) {
            printf("Weird brightness drop. Recapturing in 15 seconds.\n");
            // single call after 15s
            set_timeout(fast_timeout, main_p[TIMER_IX].fd);
        } else {
            // reset normal timer
            set_timeout(conf.timeout, main_p[TIMER_IX].fd);
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
                case TIMER_IX:
                    /* we received a timer expiration signal on timerfd */
                    read(main_p[i].fd, &t, 8);
                    do_capture();
                    break;
                case SIGNAL_IX:
                    /* we received a signal */
                    sig_handler(main_p[i].fd);
                    break;
                }
                r--;
            }
        }
    }
}
