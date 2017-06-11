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

/* 
 * Useful macro to check size of global array.
 * Used in every module to automatically set self.num_deps
 */
#define SIZE(a) sizeof(a) / sizeof(*a)

#define DONT_POLL -2                        // avoid polling a module (used for modules that do not need to be polled)
#define DONT_POLL_W_ERR -3                  // avoid polling a module because an error occurred (used eg when no geoclue2 is found)
#define SIZE_POINTS 11                      // number of points (from 0 to 10 included)
#define DEGREE 3                            // number of parameters for polynomial regression

/* List of modules indexes */
enum modules { BRIGHTNESS, LOCATION, UPOWER, GAMMA, SIGNAL, BUS, DIMMER, MODULES_NUM };

/*
 * List of states clight can be through: 
 * day between sunrise and sunset
 * night between sunset and sunrise
 * EVENT from 30mins before until 30mins after an event
 * unknown if no sunrise/sunset could be found for today (can it happen?)
 */
enum states { UNKNOWN, DAY, NIGHT, EVENT, SIZE_STATES };

/* List of events: sunrise and sunset */
enum events { SUNRISE, SUNSET, SIZE_EVENTS };

/* Whether a inter-module dep is hard (mandatory) or soft dep */
enum dep_type { HARD, SOFT };

/* Whether laptop is on battery or connected to ac */
enum ac_states { ON_AC, ON_BATTERY, SIZE_AC };

/* Struct that holds global config as passed through cmdline args */
struct config {
    int num_captures;                       // number of frame captured for each screen brightness compute
    int single_capture_mode;                // do a capture and leave
    int timeout[SIZE_AC][SIZE_STATES];      // timeout between captures for each ac_state and time state (day/night/event)
    char dev_name[PATH_MAX + 1];            // video device (eg: /dev/video0) to be used for captures
    char screen_path[PATH_MAX + 1];         // screen syspath (eg: /sys/class/backlight/intel_backlight)
    int temp[SIZE_STATES];                  // screen temperature for each state (day/night only exposed through cmdline opts)
    int no_smooth_transition;               // disable smooth transitions for gamma
    double lat;                             // latitude
    double lon;                             // longitude
    char events[SIZE_EVENTS][10];           // sunrise/sunset times passed from cmdline opts (if setted, location module won't be started)
    int no_gamma;                           // whether gamma tool is disabled
    int lowest_backlight_level;             // lowest backlight level to be setted
    int max_backlight_pct[SIZE_AC];         // max backlight percentage per-ac state (for now, only ON_BATTERY is exported though)
    int event_duration;                     // duration of an event (by default 30mins, ie: it starts 30mins before an event and ends 30mins after)
    double regression_points[SIZE_POINTS];  // points used for regression through libgsl
    int dimmer_timeout[SIZE_AC];            // dimmer timeout
    int dimmer_pct;                         // pct of max brightness to be used while dimming
    int no_dimmer;                          // disable dimmer
};

/*
 * Storage struct for our needed variables.
 */
struct brightness {
    int current;
    int max;
    int old;
};

/* Global state of program */
struct state {
    int quit;                               // should we quit?
    enum states time;                       // whether it is day or night time
    time_t events[SIZE_EVENTS];             // today events (sunrise/sunset)
    enum events next_event;                 // next event index (sunrise/sunset)
    int event_time_range;                   // variable that holds minutes in advance/after an event to enter/leave EVENT state
    enum ac_states ac_state;                // is laptop on battery?
    int fast_recapture;                     // fast recapture after huge brightness drop?
    double fit_parameters[DEGREE];          // best-fit parameters
    const char *xauthority;                 // xauthority env variable, to be used in gamma calls
    const char *display;                    // display env variable, to be used in gamma calls
    struct brightness br;                   // struct that hold screen backlight info
    int is_dimmed;                          // whether we are currently in dimmed state
};

/* Struct that holds info about an inter-modules dep */
struct dependency {
    enum dep_type type;                    // soft or hard dependency 
    enum modules dep;                      // dependency module
};

/* Struct that holds self module informations, static to each module */
struct self_t {
    const char *name;                     // name of module
    const enum modules idx;               // idx of a module in enum modules 
    int num_deps;                         // number of deps for a module
    int satisfied_deps;                   // number of satisfied deps
    struct dependency *deps;              // module on which there is a dep
};

/* Struct that holds data for each module */
struct module {
    void (*init)(void);                   // module init function
    int (*check)(void);                   // module check-before-init function
    void (*destroy)(void);                // module destroy function
    void (*poll_cb)(void);                // module poll callback
    struct self_t *self;                  // pointer to self module informations
    enum modules *dependent_m;            // pointer to every dependent module self
    int num_dependent;                    // number of dependent-on-this-module modules
    int inited;                           // whether a module has been initialized
    int disabled;                         // whether this module has been disabled from config (for now useful only for gamma)
};

struct state state;
struct config conf;
struct module modules[MODULES_NUM];
struct pollfd main_p[MODULES_NUM]; 
