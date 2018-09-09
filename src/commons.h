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
#define LOC_DISTANCE_THRS 50000             // threshold for location distances before triggering location changed events (50km)

/* List of modules indexes */
enum modules { BRIGHTNESS, LOCATION, UPOWER, GAMMA, SIGNAL, BUS, DIMMER, DPMS, XORG, INHIBIT, USERBUS, CLIGHTD, INTERFACE, MODULES_NUM };

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

/* Whether a module A on B dep is hard (mandatory), soft dep or it is its submodule */
enum dep_type { NO_DEP, HARD, SOFT, SUBMODULE };

/* Whether laptop is on battery or connected to ac */
enum ac_states { ON_AC, ON_BATTERY, SIZE_AC };

/* Dpms states */
enum dpms_states { STANDBY, SUSPEND, OFF, SIZE_DPMS };

/* Module states */
enum module_states { IDLE, STARTED_DISABLED, DISABLED, RUNNING, DESTROYED };

/* Module management operations */
enum module_op { PAUSE, RESUME, OP_NUM };

/* Bus types */
enum bus_type { SYSTEM, USER };

/* Quit values */
enum quit_values { NORM_QUIT = 1, ERR_QUIT };

/* Struct that holds data about a geographic location */
struct location {
    double lat;
    double lon;
};

/* 
 * bus_mod_idx: set in every module's match callback to their self.idx.
 * It is the idx of the module on which bus should call callbacks
 * stored in struct bus_cb *callbacks
 */
struct bus_match_data {
    int bus_mod_idx;
    void *ptr;
};

/* Struct that holds global config as passed through cmdline args/config file reading */
struct config {
    int num_captures;                       // number of frame captured for each screen brightness compute
    int timeout[SIZE_AC][SIZE_STATES];      // timeout between captures for each ac_state and time state (day/night/event)
    char dev_name[PATH_MAX + 1];            // video device (eg: /dev/video0) to be used for captures
    char screen_path[PATH_MAX + 1];         // screen syspath (eg: /sys/class/backlight/intel_backlight)
    int temp[SIZE_STATES];                  // screen temperature for each state (day/night only exposed through cmdline opts)
    struct location loc;                    // user location
    char events[SIZE_EVENTS][10];           // sunrise/sunset times passed from cmdline opts (if setted, location module won't be started)
    int event_duration;                     // duration of an event (by default 30mins, ie: it starts 30mins before an event and ends 30mins after)
    double regression_points[SIZE_AC][SIZE_POINTS];  // points used for regression through libgsl
    int dimmer_timeout[SIZE_AC];            // dimmer timeout
    double dimmer_pct;                      // pct of max brightness to be used while dimming
    int dpms_timeouts[SIZE_AC][SIZE_DPMS];  // dpms timeouts
    int verbose;                            // whether we're in verbose mode
    int no_smooth_backlight;                // disable smooth backlight changes for BRIGHTNESS module
    int no_smooth_dimmer;                   // disable smooth backlight changes for DIMMER module
    int no_smooth_gamma;                    // disable smooth gamma changes
    double backlight_trans_step;            // every backlight transition step value (in pct), used when smooth BRIGHTNESS transitions are enabled
    int gamma_trans_step;                   // every gamma transition step value, used when smooth GAMMA transitions are enabled
    double dimmer_trans_step;               // every backlight transition step value (in pct), used when smooth DIMMER transitions are enabled
    int backlight_trans_timeout;            // every backlight transition timeout value, used when smooth BRIGHTNESS transitions are enabled
    int gamma_trans_timeout;                // every gamma transition timeout value, used when smooth GAMMA transitions are enabled
    int dimmer_trans_timeout;               // every backlight transition timeout value, used when smooth DIMMER transitions are enabled
};

/* Global state of program */
struct state {
    int quit;                               // should we quit?
    enum states time;                       // whether it is day or night time
    time_t events[SIZE_EVENTS];             // today events (sunrise/sunset)
    enum ac_states ac_state;                // is laptop on battery?
    double fit_parameters[SIZE_AC][DEGREE]; // best-fit parameters
    char *xauthority;                       // xauthority env variable, to be used in gamma calls
    char *display;                          // display env variable, to be used in gamma calls
    double current_br_pct;                  // current backlight pct
    int is_dimmed;                          // whether we are currently in dimmed state
    int pm_inhibited;                       // whether powermanagement is inhibited
    jmp_buf quit_buf;                       // quit jump called by longjmp
    int needed_functional_modules;          // we need at least 1 functional module (BRIGHTNESS, GAMMA, DPMS, DIMMER) otherwise quit
    struct bus_match_data userdata;         // Data used by modules that own a match on bus
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
    int functional_module;                // whether this module offers a high-level feature
};

/* Struct that holds data for each module */
struct module {
    void (*init)(void);                   // module init function
    int (*check)(void);                   // module check-before-init function
    void (*destroy)(void);                // module destroy function
    void (*poll_cb)(void);                // module poll callback
    struct self_t *self;                  // pointer to self module informations
    enum dep_type dependent_m[MODULES_NUM];// pointer to every dependent module self
    int num_dependent;                    // number of dependent-on-this-module modules
    enum module_states state;             // state of a module
};

extern struct state state;
extern struct config conf;
extern struct module modules[MODULES_NUM];
extern struct pollfd main_p[MODULES_NUM];
