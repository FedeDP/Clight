#pragma once

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <linux/limits.h>
#include <errno.h>
#include <math.h>
#include <pwd.h>
#include <setjmp.h>
#include <module/module_easy.h>
#include <module/modules_easy.h>

#define CLIGHTD_SERVICE "org.clightd.clightd"

#define MAX_SIZE_POINTS 50                  // max number of points used for polynomial regression
#define DEF_SIZE_POINTS 11                  // default number of points used for polynomial regression
#define DEGREE 3                            // number of parameters for polynomial regression
#define IN_EVENT SIZE_STATES                // Backlight module has 1 more state: IN_EVENT
#define LAT_UNDEFINED 91.0                  // Undefined (ie: unset) value for latitude
#define LON_UNDEFINED 181.0                 // Undefined (ie: unset) value for longitude
#define MINIMUM_CLIGHTD_VERSION_MAJ 4       // Clightd minimum required maj version
#define MINIMUM_CLIGHTD_VERSION_MIN 0       // Clightd minimum required min version

/** PubSub Macros **/

#define MSG_TYPE()              const enum mod_msg_types type = *(enum mod_msg_types *)msg->ps_msg->message
#define M_PUB(topic, ptr)       m_publish(topic, ptr, sizeof(*ptr), false);

/** Log Macros **/

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define DEBUG(msg, ...) if (conf.verbose) log_message(__FILENAME__, __LINE__, 'D', msg, ##__VA_ARGS__)
#define INFO(msg, ...) log_message(__FILENAME__, __LINE__, 'I', msg, ##__VA_ARGS__)
#define WARN(msg, ...) log_message(__FILENAME__, __LINE__, 'W', msg, ##__VA_ARGS__)
/* ERROR macro will leave clight by calling longjmp */
#define ERROR(msg, ...) \
do { \
    log_message(__FILENAME__, __LINE__, 'E', msg, ##__VA_ARGS__); \
    longjmp(state.quit_buf, ERR_QUIT); \
} while (0)

/** Generic Enums **/

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
enum quit_values { NO_QUIT, NORM_QUIT, ERR_QUIT };

/* PowerManagement states */
enum pm_states { PM_OFF, PM_ON, PM_FORCED_ON };

/* Dimming transition states */
enum dim_trans { ENTER, EXIT, SIZE_DIM };

/* Type of pubsub messages */
enum mod_msg_types { 
    LOCATION_UPD, UPOWER_UPD, INHIBIT_UPD, 
    DISPLAY_UPD, TIME_UPD, EVENT_UPD, 
    TEMP_UPD, INTERFACE_TEMP, TIMEOUT_UPD,
    DO_CAPTURE, CURVE_UPD, AUTOCALIB_UPD, 
    AMBIENT_BR, CURRENT_BL, CURRENT_KBD_BL,
    CURRENT_SCR_BL, PAUSE_UPD, RESUME_UPD,
    CONTRIB_UPD
};

struct location {
    double lat;
    double lon;
};

/** PubSub messages **/

typedef struct {
    enum mod_msg_types type; /* LOCATION_UPD */ 
    struct location old;
    struct location new;
} loc_upd;

typedef struct {
    enum mod_msg_types type; /* UPOWER_UPD */
    enum ac_states old;
    enum ac_states new;
} upower_upd;

typedef struct {
    enum mod_msg_types type; /* INHIBIT_UPD */
    enum pm_states old;
    enum pm_states new;
} inhibit_upd;

typedef struct {
    enum mod_msg_types type; /* DISPLAY_UPD */
    enum display_states old;
    enum display_states new;
} display_upd;

typedef struct {
    enum mod_msg_types type; /* TIME_UPD */
    enum states old;
    enum states new;
} time_upd;

typedef struct {
    enum mod_msg_types type; /* EVENT_UPD */
    enum events old;
    enum events new;
} evt_upd;

typedef struct {
    enum mod_msg_types type; /* TEMP_UPD/INTERFACE_TEMP */
    int old;
    int new;
} temp_upd;

typedef struct {
    enum mod_msg_types type; /* TIMEOUT_UPD */
    int old;
    int new;
} timeout_upd;

typedef struct {
    enum mod_msg_types type; /* DO_CAPTURE */
} capture_upd;

typedef struct {
    enum mod_msg_types type; /* CURVE_UPDATE */
    enum ac_states state;
} curve_upd;

typedef struct {
    enum mod_msg_types type; /* AUTOCALIB_UPD */
    int old;
    int new;
} calib_upd;

typedef struct {
    enum mod_msg_types type; /* AMBIENT_BR/CURRENT_BL/CURRENT_KBD_BL */
    double curr;
} bl_upd;

typedef struct {
    enum mod_msg_types type; /* PAUSE_UPD/RESUME_UPD */
} state_upd;

typedef struct {
    enum mod_msg_types type; /* CONTRIB_UPD */
    double old;
    double new;
} contrib_upd;

/** Generic structs **/

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
    double regression_points[SIZE_AC][MAX_SIZE_POINTS];  // points used for regression through libgsl
    int num_points[SIZE_AC];                // number of points currently used for polynomial regression
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
    int screen_timeout[SIZE_AC];            // screen timeouts
    double screen_contrib;                  // how much does screen-emitted brightness affect ambient brightness (eg 0.1)
    int screen_samples;                     // number of samples used to compute average screen-emitted brightness
    int no_gamma;
    int no_backlight;
    int no_dimmer;
    int no_dpms;
    int no_inhibit;
    int no_screen;
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
    double screen_comp;                     // current screen-emitted brightness compensation
    jmp_buf quit_buf;                       // quit jump called by longjmp
    char clightd_version[32];               // Clightd found version
    char version[32];                       // Clight version
};

/** Topics data **/

/* INHIBIT topics */
extern const char *inh_topic;                       // Topic emitted on new PM inhibit state

/* UPOWER topics */
extern const char *up_topic;                        // Topic emitted on UPower state change

/* LOCATION topics */
extern const char *loc_topic;                       // Topic emitted on new location received

/* BACKLIGHT topics */
extern const char *current_bl_topic;                // Topic emitted on current backlight level change
extern const char *current_kbd_topic;               // Topic emitted on current keyboard backlight level change
extern const char *current_ab_topic;                // Topic emitted on current ambient brightness change

/* DIMMER/DPMS topics */
extern const char *display_topic;                   // Topic emitted on display state changes

/* INTERFACE topics */
extern const char *interface_temp_topic;            // Topic emitted to change current GAMMA temperature
extern const char *interface_dimmer_to_topic;       // Topic emitted to change current DIMMER timeouts
extern const char *interface_dpms_to_topic;         // Topic emitted to change current DPMS timeouts
extern const char *interface_scr_to_topic;          // Topic emitted to change current SCREEN timeouts
extern const char *interface_bl_to_topic;           // Topic emitted to change current BACKLIGHT timeouts
extern const char *interface_bl_capture;            // Topic emitted to require an autocalib to BACKLIGHT
extern const char *interface_bl_curve;              // Topic emitted to update BACKLIGHT curve for given ac state
extern const char *interface_bl_autocalib;          // Topic emitted to pause/resume automatic calibratio for BACKLIGHT
extern const char *interface_scr_contrib;           // Topic emitted to set new SCREEN contrib value

/* GAMMA topics */
extern const char *time_topic;                      // Topic emitted on time changed, ie: DAY -> NIGHT or NIGHT -> DAY
extern const char *evt_topic;                       // Topic emitted for InEvent changes
extern const char *sunrise_topic;                   // Topic emitted on new sunrise time
extern const char *sunset_topic;                    // Topic emitted on new sunset time
extern const char *temp_topic;                      // Topic emitted on new temp set

/* SCREEN topics */
extern const char *current_scr_topic;               // Topic emitted on screen emitted brightness change

/** Global state and config data **/

extern struct state state;
extern struct config conf;

/** Log function declaration **/

void log_message(const char *filename, int lineno, const char type, const char *log_msg, ...);
