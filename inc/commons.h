#define __USE_XOPEN

#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/limits.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <stdarg.h>
#include <poll.h>
#include <math.h>
#include <pwd.h>

#define DONT_POLL -2                // avoid polling a module (used for dpms taht does not need to be polled)
#define DONT_POLL_W_ERR -3          // avoid polling a module because an error occurred (used in location.c when no geoclue2 is found)

/* List of modules indexes */
enum modules { CAPTURE_IX, LOCATION_IX, GAMMA_IX, SIGNAL_IX, DPMS_IX, MODULES_NUM };
/*
 * List of states clight can be through: 
 * day between sunrise and sunset
 * night between sunset and sunrise
 * EVENT from 30mins before until 30mins after an event
 * unknown if no sunrise/sunset could be found for today (can it happen?)
 */
enum states { DAY, NIGHT, EVENT, UNKNOWN, SIZE_STATES };

/* List of events: sunrise and sunset */
enum events { SUNRISE, SUNSET, SIZE_EVENTS };

/* Struct that holds global config as passed through cmdline args */
struct config {
    int num_captures;               // number of frame captured for each screen brightness compute
    int single_capture_mode;        // do a capture and leave
    int timeout[SIZE_STATES];       // timeout between captures for each state (day/night only exposed through cmdline opts)
    char dev_name[PATH_MAX + 1];                 // video device (eg: /dev/video0) to be used for captures
    char screen_path[PATH_MAX + 1];              // screen syspath (eg: /sys/class/backlight/intel_backlight)
    int temp[SIZE_STATES];          // screen temperature for each state (day/night only exposed through cmdline opts)
    int no_smooth_transition;       // disable smooth transitions for gamma
    double lat;                     // latitude
    double lon;                     // longitude
    char events[SIZE_EVENTS][10];      // sunrise/sunset times passed from cmdline opts (if setted, location module won't be started)
    int no_gamma;                   // disable gamma support (if setted, gamma and location modules won't be started)
};

/* Global state of program */
struct state {
    int quit;                       // should we quit?
    enum states time;               // whether it is day or night time
    time_t events[SIZE_EVENTS];     // today events (sunrise/sunset)
    enum events next_event;         // next event index (sunrise/sunset)
    int event_time_range;           // variable that holds minutes in advance/after an event to enter/leave EVENT state
};

/* Struct that holds data for each module */
struct module {
    void (*destroy)(void);          // module destroy function
    void (*poll_cb)(void);          // module poll callback
    int inited;                     // whether a module has been initialized
};

struct state state;
struct config conf;
struct module modules[MODULES_NUM];
struct pollfd main_p[MODULES_NUM];
