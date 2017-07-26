#include <signal.h>
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
#include <setjmp.h>
#include <systemd/sd-bus.h>

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
enum modules { BRIGHTNESS, LOCATION, UPOWER, GAMMA, GAMMA_SMOOTH, SIGNAL, BUS, DIMMER, DIMMER_SMOOTH, DPMS, XORG, INHIBIT, USERBUS, MODULES_NUM };

/*
 * List of states clight can be through: 
 * day between sunrise and sunset
 * night between sunset and sunrise
 * EVENT from conf.event_time_range before until conf.event_time_range after an event
 * unknown if no sunrise/sunset could be found for today (can it happen?)
 */
enum states { DAY, NIGHT, EVENT, SIZE_STATES };

/* List of events: sunrise and sunset */
enum events { SUNRISE, SUNSET, SIZE_EVENTS };

/* Whether a inter-module dep is hard (mandatory) or soft dep */
enum dep_type { HARD, SOFT };

/* Whether laptop is on battery or connected to ac */
enum ac_states { ON_AC, ON_BATTERY, SIZE_AC };

/* Dpms states */
enum dpms_states { STANDBY, SUSPEND, OFF, SIZE_DPMS };

/* Module states */
enum module_states { IDLE, DISABLED, INITED };

/* Bus types */
enum bus_type { SYSTEM, USER };

/* Struct that holds global config as passed through cmdline args */
struct config {
    int num_captures;                       // number of frame captured for each screen brightness compute
    int single_capture_mode;                // do a capture and leave
    int timeout[SIZE_AC][SIZE_STATES];      // timeout between captures for each ac_state and time state (day/night/event)
    char dev_name[PATH_MAX + 1];            // video device (eg: /dev/video0) to be used for captures
    char screen_path[PATH_MAX + 1];         // screen syspath (eg: /sys/class/backlight/intel_backlight)
    int temp[SIZE_STATES];                  // screen temperature for each state (day/night only exposed through cmdline opts)
    double lat;                             // latitude
    double lon;                             // longitude
    char events[SIZE_EVENTS][10];           // sunrise/sunset times passed from cmdline opts (if setted, location module won't be started)
    int event_duration;                     // duration of an event (by default 30mins, ie: it starts 30mins before an event and ends 30mins after)
    double regression_points[SIZE_AC][SIZE_POINTS];  // points used for regression through libgsl
    int dimmer_timeout[SIZE_AC];            // dimmer timeout
    int dimmer_pct;                         // pct of max brightness to be used while dimming
    int dpms_timeouts[SIZE_AC][SIZE_DPMS];  // dpms timeouts
    int verbose;                            // whether we're in verbose mode
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
    enum ac_states old_ac_state;            // was laptop on battery?
    int fast_recapture;                     // fast recapture after huge brightness drop?
    double fit_parameters[SIZE_AC][DEGREE]; // best-fit parameters
    const char *xauthority;                 // xauthority env variable, to be used in gamma calls
    const char *display;                    // display env variable, to be used in gamma calls
    struct brightness br;                   // struct that hold screen backlight info
    int is_dimmed;                          // whether we are currently in dimmed state
    int dimmed_br;                          // backlight level when dimmed
    int pm_inhibited;                       // whether powermanagement is inhibited
    jmp_buf quit_buf;                       // quit jump called by longjmp
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
    int standalone;                       // whether this module is a standalone module, ie: it should stay enabled even if all of its dependent module gets disabled
    int enabled_single_capture;           // whether this module is enabled during single capture mode
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
    enum module_states state;             // state of a module
};

struct state state;
struct config conf;
struct module modules[MODULES_NUM];
struct pollfd main_p[MODULES_NUM];
