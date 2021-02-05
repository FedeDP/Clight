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
#include <module/modules_easy.h>

#define UNUSED __attribute__((unused))

#define MAX_SIZE_POINTS 50                  // max number of points used for polynomial regression
#define DEF_SIZE_POINTS 11                  // default number of points used for polynomial regression
#define DEGREE 3                            // number of parameters for polynomial regression

#define IN_EVENT SIZE_STATES                // Backlight module has 1 more state: IN_EVENT

#define LAT_UNDEFINED 91.0                  // Undefined (ie: unset) value for latitude
#define LON_UNDEFINED 181.0                 // Undefined (ie: unset) value for longitude
#define LOC_DISTANCE_THRS 50                // threshold for location distances before triggering location changed events (km)

#define MINIMUM_CLIGHTD_VERSION_MAJ 5       // Clightd minimum required maj version
#define MINIMUM_CLIGHTD_VERSION_MIN 2       // Clightd minimum required min version -> org.clightd.clightd.KbdBacklight

/** Generic structs **/

typedef struct {
    int num_captures[SIZE_AC];
    char dev_name[PATH_MAX + 1];
    char dev_opts[NAME_MAX + 1];
    double regression_points[SIZE_AC][MAX_SIZE_POINTS];  // points used for regression through libgsl
    int num_points[SIZE_AC];                // number of points currently used for polynomial regression
} sensor_conf_t;

typedef struct {
    int disabled;
    int timeout[SIZE_AC][SIZE_STATES + 1];
    char screen_path[PATH_MAX + 1];         // screen syspath (eg: /sys/class/backlight/intel_backlight)
    int no_smooth;                          // disable smooth backlight changes for BACKLIGHT module
    double trans_step;                      // every backlight transition step value (in pct), used when smooth BACKLIGHT transitions are enabled
    int trans_timeout;                      // every backlight transition timeout value, used when smooth BACKLIGHT transitions are enabled
    int no_auto_calib;                      // disable automatic calibration for both BACKLIGHT and GAMMA
    double shutter_threshold;               // capture values below this threshold will be considered "shuttered"
    int pause_on_lid_closed;                // whether clight should inhibit autocalibration on lid closed
    int capture_on_lid_opened;              // whether to trigger a new capture whenever lid gets opened
} bl_conf_t;

typedef struct {
    int disabled;                           // disable keyboard backlight automatic calibration (where supported)
    int dim;                                // whether DPMS/Dimmer should switch keyboard off
    double amb_br_thres;                    // Ambient brightness high threshold
    int timeout[SIZE_AC];                   // kbd dimming timeout
} kbd_conf_t;

typedef struct {
    int disabled;
    int temp[SIZE_STATES];                  // screen temperature for each daytime
    int no_smooth;                          // disable smooth gamma changes
    int trans_step;                         // every gamma transition step value, used when smooth GAMMA transitions are enabled
    int trans_timeout;                      // every gamma transition timeout value, used when smooth GAMMA transitions are enabled
    int long_transition;                    // flag to enable a very long smooth transition for gamma (redshift-like)
    int ambient_gamma;                      // enable gamma adjustments based on ambient backlight
} gamma_conf_t;

typedef struct {
    char day_events[SIZE_EVENTS][10];       // sunrise/sunset times passed from cmdline opts (if setted, location module won't be started)
    int event_duration;                     // duration of an event (by default 30mins, ie: it starts 30mins before an event and ends 30mins after)
    loc_t loc;                              // user location as loaded by config
} daytime_conf_t;

typedef struct {
    int disabled;
    double dimmed_pct;                      // pct of max backlight to be used while dimming
    int timeout[SIZE_AC];                   // dimmer timeout
    int no_smooth[SIZE_DIM];                // disable smooth backlight changes for DIMMER module
    double trans_step[SIZE_DIM];            // every backlight transition step value (in pct), used when smooth DIMMER transitions are enabled
    int trans_timeout[SIZE_DIM];            // every backlight transition timeout value, used when smooth DIMMER transitions are enabled
} dimmer_conf_t;

typedef struct {
    int disabled;
    int timeout[SIZE_AC];                   // dpms timeouts
} dpms_conf_t;

typedef struct {
    int disabled;
    int timeout[SIZE_AC];                   // screen timeouts
    double contrib;                         // how much does screen-emitted brightness affect ambient brightness (eg 0.1)
    int samples;                            // number of samples used to compute average screen-emitted brightness
} screen_conf_t;

typedef struct {
    int disabled;
    int inhibit_docked;                     // whether to manage "docked" states as inhibitions. Requires UPower.
    int inhibit_pm;                         // whether handle inhibition suppressing powermanagement too
} inh_conf_t;

/* Struct that holds global config as passed through cmdline args/config file reading */
typedef struct {
    bl_conf_t bl_conf;
    sensor_conf_t sens_conf;
    kbd_conf_t kbd_conf;
    gamma_conf_t gamma_conf;
    daytime_conf_t day_conf;
    dimmer_conf_t dim_conf;
    dpms_conf_t dpms_conf;
    screen_conf_t screen_conf;
    inh_conf_t inh_conf;
    int verbose;                            // whether verbose mode is enabled
    int wizard;                             // whether wizard mode is enabled
    int resumedelay;                        // delay on resume from suspend
} conf_t;

/* Global state of program */

/*
 * Using INT for BOOLeans: https://dbus.freedesktop.org/doc/dbus-specification.html
 * "BOOLEAN values are encoded in 32 bits (of which only the least significant bit is used)."
 * 
 * Where bool type is 1B large, using bool would break this convention.
 */
typedef struct {
    bool looping;
    int in_event;                           // Whether we are in a TIME event +- conf.event_duration
    int inhibited;                          // whether screensaver inhibition is enabled
    int pm_inhibited;                       // whether pm_inhibition is enabled
    int sens_avail;                         // whether a sensor is currently available
    int suspended;                          // whether system is suspended
    enum day_states day_time;               // whether it is day or night time
    enum ac_states ac_state;                // is laptop on battery?
    enum lid_states lid_state;              // current lid state
    enum display_states display_state;      // current display state
    enum day_events next_event;             // next daytime event (SUNRISE or SUNSET)
    int event_time_range;
    int current_temp;                       // current GAMMA temp; specially useful when used with conf.ambient_gamma enabled
    time_t day_events[SIZE_EVENTS];         // today events (sunrise/sunset)
    loc_t current_loc;                      // current user location
    double fit_parameters[SIZE_AC][DEGREE]; // best-fit parameters for each sensor, for each AC state
    double current_bl_pct;                  // current backlight pct
    double current_kbd_pct;                 // current keyboard backlight pct
    double ambient_br;                      // last ambient brightness captured from CLIGHTD Sensor
    double screen_comp;                     // current screen-emitted brightness compensation
    const char *clightd_version;            // Clightd found version
    const char *version;                    // Clight version
    jmp_buf quit_buf;                       // quit jump called by longjmp
} state_t;

/** Global state and config data **/

extern state_t state;
extern conf_t conf;
