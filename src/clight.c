#include <poll.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <math.h>
#include <xcb/dpms.h>
#include <getopt.h>

#include "../inc/camera.h"
#include "../inc/config.h"

#define TIMER_IX 0
#define SIGNAL_IX 1
#define CAMERA_IX 2

static const int fast_timeout = 15;
static const int nfds = 3;
static const double drop_limit = 0.6;

static int dpms_enabled, single_capture_mode;
static struct pollfd *main_p;
static xcb_connection_t *connection;

static void init_config(int argc, char *argv[]);
static void setup_everything(void);
static void parse_cmd(int argc, char * const argv[]);
static void print_help(void);
static void do_single_capture(void);
static void init_dpms(void);
static int set_signals(void);
static void set_pollfd(void);
static void free_everything(void);
static int start_timer(void);
static void set_timeout(int start, int fd);
static void sig_handler(int fd);
static void timer_func(void);
static void camera_func(void);
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
    strncpy(conf.dev_name, "/dev/video0", PATH_MAX);
    strncpy(conf.screen_path, "/sys/class/backlight/intel_backlight", PATH_MAX);
    
    init_config_file();
    read_config();
    parse_cmd(argc, argv);
}

static void setup_everything(void) {
    if (!quit) {
        init_brightness();
        if (!quit) {
            open_device();
        }
    
        if (!quit) {
            _log(stdout, "Using %d frames captures with timeout %d.\n", conf.num_captures, conf.timeout);
            if (single_capture_mode) {
                do_single_capture();
                quit = 1;
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
            _log(stdout, "Entered fast capture mode.\n");
            break;
        case 'f':
            conf.num_captures =  atoi(optarg);
            break;
        case 't':
            conf.timeout = atoi(optarg);
            break;
        case 's':
            setup_config();
            quit = 1;
            break;
        case 'd':
            strncpy(conf.dev_name, optarg, PATH_MAX);
            break;
        case 'b':
            strncpy(conf.screen_path, optarg, PATH_MAX);
            break;
        case 'h':
            print_help();
            quit = 1;
            return;
        case '?':
            quit = 1;
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
 * When in single_capture_mode, this function will
 * just do num_captures frame captures and
 * set the correct brightness level, then leave.
 */
static void do_single_capture(void) {
    if (start_stream() == -1) {
        quit = 1;
    }
    for (int i = 0; i < conf.num_captures && !quit; i++) {
        camera_func();
    }
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
    main_p = malloc(nfds * sizeof(struct pollfd));
    int timerfd = start_timer();
    if (timerfd == -1 || !main_p) {
        quit = 1;
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
    main_p[CAMERA_IX] = (struct pollfd) {
        .fd = -1,
        .events = POLLIN,
    };
}

/**
 * Free every resource used
 */
static void free_everything(void) {
    if (main_p) {
        // CAMERA_IX will be freed by free_device
        for (int i = 0; i < SIGNAL_IX; i++) {
            close(main_p[i].fd);
        }
        free(main_p);
    }
    
    free_device();
    free_brightness();
    
    if (connection) {
        xcb_disconnect(connection);
    }
}

/**
 * Create 30s timer and returns its fd to
 * the main struct pollfd
 */
static int start_timer(void) {
    int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (timerfd == -1) {
        _log(stderr, "could not start timer.\n");
        return -1;
    }
    _log(stdout, "Started timer.\n");
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
    
    _log(stdout, "Timer expiring in: %d seconds.\n", start);
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
        _log(stderr, "an error occurred while getting signalfd data.\n");
    } else {
        _log(stdout, "received signal %d. Leaving.\n", fdsi.ssi_signo);
    }
    quit = 1;
}

/**
 * When timerfd timeout expires, check if we are in screen power_save mode,
 * otherwise start streaming on webcam and set CAMERA_IX fd of pollfd struct to
 * webcam device fd. This way our main poll will get events (frames) from webcam device too.
 */
static void timer_func(void) {
    // if screen is currently blanked thanks to dpms,
    // do not do anything. Set a long timeout and return.
    // Timeout will increase as screen power management goes deeper.
    if (dpms_enabled && get_screen_dpms() > 0) {
        set_timeout(2 * conf.timeout * get_screen_dpms(), main_p[TIMER_IX].fd);
        return;
    }
    
    int ret = start_stream();
    if (ret == -1) {
        quit = 1;
    } else {
        // enable camera catching on poll
        main_p[CAMERA_IX].fd = ret;
    }
}

/**
 * When receiving an event on CAMERA_IX fd,
 * capture a frame. If the captured frame is the last one,
 * stop streaming and compute new backlight.
 * Finally, set it and check current backlight drop.
 * If it is too high, schedule a fast new check (15s)
 */
static void camera_func(void) {
    int ret = capture_frame();
    if (ret == -1) {
        quit = 1;
    } else if (ret == conf.num_captures) {
        if (stop_stream() == -1) {
            quit = 1;
        } else {
            double val = compute_backlight();
            double drop = 0;
            
            if (val != 0) {
                drop = set_brightness(val);
            }
            
            if (!single_capture_mode) {
                // disable catching on poll
                main_p[CAMERA_IX].fd = -1;
                
                // if there is too high difference, do a fast recapture to be sure
                // this is the correct level
                if (fabsf(drop) > drop_limit) {
                    _log(stdout, "Weird brightness drop. Recapturing in 15 seconds.\n");
                    // single call after 15s
                    set_timeout(fast_timeout, main_p[TIMER_IX].fd);
                } else {
                    // reset normal timer
                    set_timeout(conf.timeout, main_p[TIMER_IX].fd);
                }
            }
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
    
    while (!quit) {
        int r = poll(main_p, nfds, -1);
        if (r == -1) {
            quit = 1;
            return;
        }
            
        for (int i = 0; i < nfds && r > 0; i++) {
            if (main_p[i].revents & POLLIN) {
                switch (i) {
                case TIMER_IX:
                    /* we received a timer expiration signal on timerfd */
                    read(main_p[i].fd, &t, 8);
                    timer_func();
                    break;
                case SIGNAL_IX:
                    /* we received a signal */
                    sig_handler(main_p[i].fd);
                    break;
                case CAMERA_IX:
                    /* we received a camera frame */
                    camera_func();
                }
                r--;
            }
        }
    }
}
