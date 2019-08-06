#pragma once

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
#include <module/module_easy.h>
#include <module/modules_easy.h>

#include "topics.h"

/*
 * Useful macro to check size of global array.
 * Used in every module to automatically set self.num_deps
 */
#define SIZE(a) sizeof(a) / sizeof(*a)

#define CLIGHTD_SERVICE "org.clightd.clightd"

#define DONT_POLL -2                        // avoid polling a module (used for modules that do not need to be polled)
#define DONT_POLL_W_ERR -3                  // avoid polling a module because an error occurred (used eg when no geoclue2 is found)
#define SIZE_POINTS 11                      // number of points (from 0 to 10 included)
#define DEGREE 3                            // number of parameters for polynomial regression
#define LOC_TIME_THRS 600                   // time threshold (seconds) before triggering location changed events (10mins)
#define LOC_DISTANCE_THRS 50000             // threshold for location distances before triggering location changed events (50km)
#define GAMMA_LONG_TRANS_TIMEOUT 10         // 10s between each step with slow transitioning
#define IN_EVENT SIZE_STATES                // Backlight module has 1 more state: IN_EVENT
#define LAT_UNDEFINED 91.0
#define LON_UNDEFINED 181.0
#define MINIMUM_CLIGHTD_VERSION_MAJ 4
#define MINIMUM_CLIGHTD_VERSION_MIN 0

#define MSG_TYPE()  const enum mod_msg_types type = *(enum mod_msg_types *)msg->ps_msg->message

/*
 * List of states clight can be through: 
 * day between sunrise and sunset
 * night between sunset and sunrise
 */
enum states { DAY, NIGHT, SIZE_STATES };

/* List of events: sunrise and sunset */
enum events { SUNRISE, SUNSET, SIZE_EVENTS };

/* Whether laptop is on battery or connected to ac */
enum ac_states { ON_AC, ON_BATTERY, SIZE_AC };

/* Display states */
enum display_states { DISPLAY_ON, DISPLAY_DIMMED, DISPLAY_OFF };

/* Quit values */
enum quit_values { NORM_QUIT = 1, ERR_QUIT };

enum pm_states { PM_OFF, PM_ON, PM_FORCED_ON };

enum dim_trans { ENTER, EXIT, SIZE_DIM };

enum mod_msg_types { 
    LOCATION_UPDATE, UPOWER_UPDATE, INHIBIT_UPDATE, CURRENT_BL, 
    DISPLAY_UPDATE, INTERFACE_TEMP, TIMEOUT_UPDATE, 
    TIME_UPDATE, EVENT_UPDATE, TEMP_UPDATE, DO_CAPTURE,
    CURVE_UPDATE, AUTOCALIB_UPD, CURRENT_KBD_BL, AMBIENT_BR, 
    PAUSE_UPD, RESUME_UPD
};

/* Struct that holds data about a geographic location */
struct location {
    double lat;
    double lon;
};

typedef struct {
    enum mod_msg_types type;
    struct location old;
    struct location new;
} loc_upd;

typedef struct {
    enum mod_msg_types type;
    enum ac_states old;
    enum ac_states new;
} upower_upd;

typedef struct {
    enum mod_msg_types type;
    enum pm_states old;
    enum pm_states new;
} inhibit_upd;

typedef struct {
    enum mod_msg_types type;
    enum display_states old;
    enum display_states new;
} display_upd;

typedef struct {
    enum mod_msg_types type;
    enum states old;
    enum states new;
} time_upd;

typedef struct {
    enum mod_msg_types type;
    enum events old;
    enum events new;
} evt_upd;

typedef struct {
    enum mod_msg_types type;
    int old;
    int new;
} temp_upd;

typedef struct {
    enum mod_msg_types type;
    int old;
    int new;
} timeout_upd;

typedef struct {
    enum mod_msg_types type;
} capture_upd;

typedef struct {
    enum mod_msg_types type;
    enum ac_states state;
} curve_upd;

typedef struct {
    enum mod_msg_types type;
    int old;
    int new;
} calib_upd;

typedef struct {
    enum mod_msg_types type;
    double curr;
} bl_upd;

typedef struct {
    enum mod_msg_types type;
} state_upd;

/* Struct that holds global config as passed through cmdline args/config file reading */
struct config {
    int num_captures;                       // number of frame captured for each screen backlight compute
    int timeout[SIZE_AC][SIZE_STATES + 1];  // timeout between captures for each ac_state and time state (day/night + during event)
    char dev_name[PATH_MAX + 1];            // video device (eg: /dev/video0) to be used for captures
    char screen_path[PATH_MAX + 1];         // screen syspath (eg: /sys/class/backlight/intel_backlight)
    int temp[SIZE_STATES];                  // screen temperature for each state
    struct location loc;                    // user location as loaded by config
    char events[SIZE_EVENTS][10];           // sunrise/sunset times passed from cmdline opts (if setted, location module won't be started)
    int event_duration;                     // duration of an event (by default 30mins, ie: it starts 30mins before an event and ends 30mins after)
    double regression_points[SIZE_AC][SIZE_POINTS];  // points used for regression through libgsl
    int dimmer_timeout[SIZE_AC];            // dimmer timeout
    double dimmer_pct;                      // pct of max backlight to be used while dimming
    int dpms_timeout[SIZE_AC];              // dpms timeouts
    int verbose;                            // whether we're in verbose mode
    int no_smooth_backlight;                // disable smooth backlight changes for BACKLIGHT module
    int no_smooth_dimmer[SIZE_DIM];         // disable smooth backlight changes for DIMMER module
    int no_smooth_gamma;                    // disable smooth gamma changes
    double backlight_trans_step;            // every backlight transition step value (in pct), used when smooth BACKLIGHT transitions are enabled
    int gamma_trans_step;                   // every gamma transition step value, used when smooth GAMMA transitions are enabled
    double dimmer_trans_step[SIZE_DIM];     // every backlight transition step value (in pct), used when smooth DIMMER transitions are enabled
    int backlight_trans_timeout;            // every backlight transition timeout value, used when smooth BACKLIGHT transitions are enabled
    int gamma_trans_timeout;                // every gamma transition timeout value, used when smooth GAMMA transitions are enabled
    int dimmer_trans_timeout[SIZE_DIM];     // every backlight transition timeout value, used when smooth DIMMER transitions are enabled
    int no_auto_calib;                      // disable automatic calibration for both BACKLIGHT and GAMMA
    int no_keyboard_bl;                     // disable keyboard backlight automatic calibration (where supported)
    double shutter_threshold;               // capture values below this threshold will be considered "shuttered"
    int gamma_long_transition;              // flag to enable a very long smooth transition for gamma (redshift-like)
    int ambient_gamma;                      // enable gamma adjustments based on ambient backlight
    int no_gamma;
    int no_backlight;
    int no_dimmer;
    int no_dpms;
    int no_inhibit;
};

/* Global state of program */
struct state {
    int quit;                               // should we quit?
    enum states time;                       // whether it is day or night time
    int in_event;                           // Whether we are in a TIME event +- conf.event_duration
    time_t events[SIZE_EVENTS];             // today events (sunrise/sunset)
    enum ac_states ac_state;                // is laptop on battery?
    double fit_parameters[SIZE_AC][DEGREE]; // best-fit parameters
    char *xauthority;                       // xauthority env variable
    char *display;                          // DISPLAY env variable
    char *wl_display;                       // WAYLAND_DISPLAY env variable
    double current_bl_pct;                  // current backlight pct
    double current_kbd_pct;                 // current keyboard backlight pct
    int current_temp;                       // current GAMMA temp; specially useful when used with conf.ambient_gamma enabled
    double ambient_br;                      // last ambient brightness captured from CLIGHTD Sensor
    enum display_states display_state;      // current display state
    enum pm_states pm_inhibited;            // whether powermanagement is inhibited
    struct location current_loc;            // current user location
    jmp_buf quit_buf;                       // quit jump called by longjmp
    char clightd_version[PATH_MAX + 1];     // Clightd found version
    char *version;                          // Clight version
};

extern struct state state;
extern struct config conf;
