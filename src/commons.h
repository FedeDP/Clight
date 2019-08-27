#pragma once

#include <stdlib.h>
#include <time.h>
#include <linux/limits.h>
#include <errno.h>
#include <math.h>
#include <pwd.h>
#include <setjmp.h>
#include "public.h"
#include "validations.h"

#define UNUSED __attribute__((unused))

#define MAX_SIZE_POINTS 50                  // max number of points used for polynomial regression
#define DEF_SIZE_POINTS 11                  // default number of points used for polynomial regression
#define DEGREE 3                            // number of parameters for polynomial regression
#define IN_EVENT SIZE_STATES                // Backlight module has 1 more state: IN_EVENT
#define LAT_UNDEFINED 91.0                  // Undefined (ie: unset) value for latitude
#define LON_UNDEFINED 181.0                 // Undefined (ie: unset) value for longitude
#define MINIMUM_CLIGHTD_VERSION_MAJ 4       // Clightd minimum required maj version
#define MINIMUM_CLIGHTD_VERSION_MIN 0       // Clightd minimum required min version

/* ERROR macro will leave clight by calling longjmp; thus it is not exposed to public header */
#define ERROR(msg, ...) \
do { \
    log_message(__FILENAME__, __LINE__, 'E', msg, ##__VA_ARGS__); \
    longjmp(state.quit_buf, ERR_QUIT); \
} while (0)

/** Generic structs **/

/* Struct that holds global config as passed through cmdline args/config file reading */
typedef struct {
    int num_captures;                       // number of frame captured for each screen backlight compute
    int timeout[SIZE_AC][SIZE_STATES + 1];  // timeout between captures for each ac_state and time state (day/night + during event)
    char dev_name[PATH_MAX + 1];            // video device (eg: /dev/video0) to be used for captures
    char screen_path[PATH_MAX + 1];         // screen syspath (eg: /sys/class/backlight/intel_backlight)
    int temp[SIZE_STATES];                  // screen temperature for each state
    loc_t loc;                              // user location as loaded by config
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
    int inhibit_autocalib;                  // whether to inhibit backlight autocalibration too when Screensaver inhibition is enabled
    int screen_timeout[SIZE_AC];            // screen timeouts
    double screen_contrib;                  // how much does screen-emitted brightness affect ambient brightness (eg 0.1)
    int screen_samples;                     // number of samples used to compute average screen-emitted brightness
    int no_gamma;
    int no_backlight;
    int no_dimmer;
    int no_dpms;
    int no_screen;
} conf_t;

/* Global state of program */
typedef struct {
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
    bool inhibited;                         // whether screensaver inhibition is enabled
    loc_t current_loc;                      // current user location
    double screen_comp;                     // current screen-emitted brightness compensation
    jmp_buf quit_buf;                       // quit jump called by longjmp
    char clightd_version[32];               // Clightd found version
    char version[32];                       // Clight version
} state_t;

/** Global state and config data **/

extern state_t state;
extern conf_t conf;
