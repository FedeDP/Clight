#pragma once

#include <stdlib.h>
#include <time.h>
#include <linux/limits.h>
#include <errno.h>
#include <math.h>
#include <pwd.h>
#include "public.h"
#include "validations.h"
#include "log.h"

#define UNUSED __attribute__((unused))
#define MAX_SIZE_POINTS 50                  // max number of points used for polynomial regression
#define DEF_SIZE_POINTS 11                  // default number of points used for polynomial regression
#define DEGREE 3                            // number of parameters for polynomial regression
#define IN_EVENT SIZE_STATES                // Backlight module has 1 more state: IN_EVENT
#define LAT_UNDEFINED 91.0                  // Undefined (ie: unset) value for latitude
#define LON_UNDEFINED 181.0                 // Undefined (ie: unset) value for longitude
#define MINIMUM_CLIGHTD_VERSION_MAJ 4       // Clightd minimum required maj version
#define MINIMUM_CLIGHTD_VERSION_MIN 0       // Clightd minimum required min version

/** Generic structs **/

typedef struct {
    int num_captures[SIZE_AC];
    char dev_name[PATH_MAX + 1];
    char dev_opts[NAME_MAX + 1];
    double regression_points[SIZE_AC][MAX_SIZE_POINTS];  // points used for regression through libgsl
    int num_points[SIZE_AC];                // number of points currently used for polynomial regression
} sensor_conf_t;

typedef struct {
    int no_backlight;
    int timeout[SIZE_AC][SIZE_STATES + 1];
    char screen_path[PATH_MAX + 1];         // screen syspath (eg: /sys/class/backlight/intel_backlight)
    int no_smooth_backlight;                // disable smooth backlight changes for BACKLIGHT module
    double backlight_trans_step;            // every backlight transition step value (in pct), used when smooth BACKLIGHT transitions are enabled
    int backlight_trans_timeout;            // every backlight transition timeout value, used when smooth BACKLIGHT transitions are enabled
    int no_auto_calib;                      // disable automatic calibration for both BACKLIGHT and GAMMA
    int no_keyboard_bl;                     // disable keyboard backlight automatic calibration (where supported)
    double shutter_threshold;               // capture values below this threshold will be considered "shuttered"
    int inhibit_calib_on_lid_closed;        // whether clight should inhibit autocalibration on lid closed
    int dim_kbd;                            // whether DPMS/Dimmer should switch keyboard off
} bl_conf_t;

typedef struct {
    int no_gamma;
    int temp[SIZE_STATES];                  // screen temperature for each daytime
    char day_events[SIZE_EVENTS][10];       // sunrise/sunset times passed from cmdline opts (if setted, location module won't be started)
    int event_duration;                     // duration of an event (by default 30mins, ie: it starts 30mins before an event and ends 30mins after)
    int no_smooth_gamma;                    // disable smooth gamma changes
    int gamma_trans_step;                   // every gamma transition step value, used when smooth GAMMA transitions are enabled
    int gamma_trans_timeout;                // every gamma transition timeout value, used when smooth GAMMA transitions are enabled
    int gamma_long_transition;              // flag to enable a very long smooth transition for gamma (redshift-like)
    int ambient_gamma;                      // enable gamma adjustments based on ambient backlight
    loc_t loc;                              // user location as loaded by config
} gamma_conf_t;

typedef struct {
    int no_dimmer;
    double dimmer_pct;                      // pct of max backlight to be used while dimming
    int dimmer_timeout[SIZE_AC];            // dimmer timeout
    int no_smooth_dimmer[SIZE_DIM];         // disable smooth backlight changes for DIMMER module
    double dimmer_trans_step[SIZE_DIM];     // every backlight transition step value (in pct), used when smooth DIMMER transitions are enabled
    int dimmer_trans_timeout[SIZE_DIM];     // every backlight transition timeout value, used when smooth DIMMER transitions are enabled
} dimmer_conf_t;

typedef struct {
    int no_dpms;
    int dpms_timeout[SIZE_AC];              // dpms timeouts
} dpms_conf_t;

typedef struct {
    int no_screen;
    int screen_timeout[SIZE_AC];            // screen timeouts
    double screen_contrib;                  // how much does screen-emitted brightness affect ambient brightness (eg 0.1)
    int screen_samples;                     // number of samples used to compute average screen-emitted brightness
} screen_conf_t;

/* Struct that holds global config as passed through cmdline args/config file reading */
typedef struct {
    bl_conf_t bl_conf;
    sensor_conf_t sens_conf;
    gamma_conf_t gamma_conf;
    dimmer_conf_t dim_conf;
    dpms_conf_t dpms_conf;
    screen_conf_t screen_conf;
    int verbose;                            // whether verbose mode is enabled
    int inhibit_docked;                     // whether to manage "docked" states as inhibitions. Requires UPower.
} conf_t;

/* Global state of program */
typedef struct {
    int quit;                               // should we quit?
    enum day_states day_time;               // whether it is day or night time
    int in_event;                           // Whether we are in a TIME event +- conf.event_duration
    time_t day_events[SIZE_EVENTS];         // today events (sunrise/sunset)
    enum ac_states ac_state;                // is laptop on battery?
    enum lid_states lid_state;               // current lid state
    double fit_parameters[SIZE_AC][DEGREE]; // best-fit parameters for each sensor, for each AC state
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

/* Initialized by backlight.c */
extern const char *sensor_names[];
