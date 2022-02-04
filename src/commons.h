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
#include <module/map.h>

#define UNUSED __attribute__((unused))

#define MAX_SIZE_POINTS 50                  // max number of points used for polynomial regression
#define DEF_SIZE_POINTS 11                  // default number of points used for polynomial regression
#define DEGREE 3                            // number of parameters for polynomial regression

#define IN_EVENT SIZE_STATES                // Backlight module has 1 more state: IN_EVENT

#define LAT_UNDEFINED 91.0                  // Undefined (ie: unset) value for latitude
#define LON_UNDEFINED 181.0                 // Undefined (ie: unset) value for longitude
#define LOC_DISTANCE_THRS 50                // threshold for location distances before triggering location changed events (km)

#define MINIMUM_CLIGHTD_VERSION_MAJ 5       // Clightd minimum required maj version
#define MINIMUM_CLIGHTD_VERSION_MIN 5       // Clightd minimum required min version -> org.clightd.clightd.Backlight2

typedef struct {
    int no_smooth;                          // disable smooth backlight changes for module
    double trans_step;                      // every backlight transition step value (in pct), used when smooth transitions are enabled
    int trans_timeout;                      // every backlight transition timeout value, used when smooth transitions are enabled
    int trans_fixed;                        // fixed transitions duration
} bl_smooth_t;

typedef struct {
    int num_points;
    double points[MAX_SIZE_POINTS];
    double fit_parameters[DEGREE]; // best-fit parameters
} curve_t;

/*
 * We need this to mark option to be parsed as bool from libconfig (ie: "x = true;"); 
 * but both dbus and libconfig expect an int-sized type below a bool.
 */
typedef unsigned int bool_to_int;

#define X_GENERIC_CONF \
    X_CONF(bool_to_int, verbose, 0); \
    X_CONF(int, resumedelay, 0);

#define X_SENS_CONF \
    X_CONF(const char *, devname, NULL); \
    X_CONF(const char *, settings, NULL);

#define X_BL_CONF \
    X_CONF(bool_to_int, disabled, false); \
    X_CONF(bool_to_int, no_auto_calibration, false); \
    X_CONF(double, shutter_threshold, 0.0f); \
    X_CONF(bool_to_int, pause_on_lid_closed, false); \
    X_CONF(bool_to_int, capture_on_lid_opened, false); \
    X_CONF(bool_to_int, restore_on_exit, false); \
    X_CONF(bool_to_int, no_smooth_transition, false); \
    X_CONF(double, trans_step, 0.05);  \
    X_CONF(int, trans_timeout, 30); \
    X_CONF(int, trans_fixed, 0);

#define X_KBD_CONF \
    X_CONF(bool_to_int, disabled, false); \
    X_CONF(bool_to_int, dim, false);

#define X_GAMMA_CONF \
    X_CONF(bool_to_int, disabled, false); \
    X_CONF(bool_to_int, no_smooth_transition, false); \
    X_CONF(int, trans_step, 50);  \
    X_CONF(int, trans_timeout, 300); \
    X_CONF(bool_to_int, long_transition, false); \
    X_CONF(bool_to_int, ambient_gamma, false); \
    X_CONF(bool_to_int, restore_on_exit, false);
    
#define X_DAYTIME_CONF \
    X_CONF(int, event_duration, 30 * 60);
    
#define X_DIMMER_CONF \
    X_CONF(bool_to_int, disabled, false); \
    X_CONF(double, dimmed_pct, 0.2);
    
#define X_DPMS_CONF \
    X_CONF(bool_to_int, disabled, false);
    
#define X_SCREEN_CONF \
    X_CONF(bool_to_int, disabled, false); \
    X_CONF(double, contrib, 0.1); \
    X_CONF(int, num_samples, 10);
    
#define X_INH_CONF \
    X_CONF(bool_to_int, disabled, false); \
    X_CONF(bool_to_int, inhibit_docked, false); \
    X_CONF(bool_to_int, inhibit_pm, false); \
    X_CONF(bool_to_int, inhibit_bl, false);

#define X_CONF(tp, name, def) tp name

/** Generic structs **/

typedef struct {
    X_SENS_CONF
    int num_captures[SIZE_AC];
    curve_t default_curve[SIZE_AC];                     // points used for regression through libgsl
    map_t *specific_curves;                             // map of monitor-specific curves
} sensor_conf_t;

typedef struct {
    X_BL_CONF
    int timeout[SIZE_AC][SIZE_STATES + 1];              // Complex types must be manually parsed
} bl_conf_t;

typedef struct {
    X_KBD_CONF
    int timeout[SIZE_AC];                   // kbd dimming timeout
    curve_t curve[SIZE_AC];                 // curve used to match ambient brightness to certain keyboard backlight level
} kbd_conf_t;

typedef struct {
    X_GAMMA_CONF
    int temp[SIZE_STATES];                  // screen temperature for each daytime
} gamma_conf_t;

typedef struct {
    X_DAYTIME_CONF
    char day_events[SIZE_EVENTS][10];       // sunrise/sunset times passed from cmdline opts (if setted, location module won't be started)
    loc_t loc;                              // user location as loaded by config
    int events_os[SIZE_EVENTS];             // offset for each event
} daytime_conf_t;

typedef struct {
    X_DIMMER_CONF
    int timeout[SIZE_AC];                   // dimmer timeout
    bl_smooth_t smooth[SIZE_DIM];
} dimmer_conf_t;

typedef struct {
    X_DPMS_CONF
    int timeout[SIZE_AC];                   // dpms timeouts
} dpms_conf_t;

typedef struct {
    X_SCREEN_CONF
    int timeout[SIZE_AC];                   // screen timeouts
} screen_conf_t;

typedef struct {
    X_INH_CONF
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
    X_GENERIC_CONF
    int wizard;                             // whether wizard mode is enabled
} conf_t;

#undef X_CONF

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
