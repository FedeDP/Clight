#pragma once

#include <string.h>
#include <module/module_easy.h>

#define MAX_SIZE_POINTS 50                  // max number of points used for polynomial regression

/** PubSub Macros **/

#define MSG_TYPE()              const enum mod_msg_types type = *(enum mod_msg_types *)msg->ps_msg->message
#define M_PUB(ptr)              m_publish(topics[(ptr)->type], ptr, sizeof(*(ptr)), false);
#define M_SUB(type)             m_subscribe(topics[type]);

/** Log Macros **/

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define DEBUG(msg, ...) if (conf.verbose) log_message(__FILENAME__, __LINE__, 'D', msg, ##__VA_ARGS__)
#define INFO(msg, ...) log_message(__FILENAME__, __LINE__, 'I', msg, ##__VA_ARGS__)
#define WARN(msg, ...) log_message(__FILENAME__, __LINE__, 'W', msg, ##__VA_ARGS__)

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

/* Dimming transition states */
enum dim_trans { ENTER, EXIT, SIZE_DIM };

/* Type of pubsub messages */

/* You should only subscribe on _UPD, and publish on _REQ */
enum mod_msg_types {
    LOCATION_UPD,       // Subscribe to receive new locations
    UPOWER_UPD,         // Subscribe to receive new AC states
    INHIBIT_UPD,        // Subscribe to receive new PowerManagement states
    DISPLAY_UPD,        // Subscribe to receive new display states (on/dimmed/off)
    TIME_UPD,           // Subscribe to receive new time states (day/night)
    EVENT_UPD,          // Subscribe to receive new InEvent states
    SUNRISE_UPD,        // Subscribe to receive new Sunrise times
    SUNSET_UPD,         // Subscribe to receive new Sunset times
    TEMP_UPD,           // Subscribe to receive new gamma temperatures
    AMBIENT_BR_UPD,     // Subscribe to receive new ambient brightness values
    BL_UPD,             // Subscribe to receive new backlight level values
    KBD_BL_UPD,         // Subscribe to receive new keyboard backlight values
    SCR_BL_UPD,         // Subscribe to receive new screen-emitted brightness values
    LOCATION_REQ,       // Publish to set a new location
    UPOWER_REQ,         // Publish to set a new UPower state
    INHIBIT_REQ,        // Publish to set a new PowerManagement state
    TEMP_REQ,           // Publish to set a new gamma temp
    BL_REQ,             // Publish to set a new backlight level
    KBD_BL_REQ,         // Publish to set a new keyboard backlight level
    DIMMER_TO_REQ,      // Publish to set a new dimmer timeout
    DPMS_TO_REQ,        // Publish to set a new dpms timeout
    SCR_TO_REQ,         // Publish to set a new screen timeout
    BL_TO_REQ,          // Publish to set a new backlight timeout
    CAPTURE_REQ,        // Publish to set a new backlight calibration
    CURVE_REQ,          // Publish to set a new backlight curve for given ac state
    AUTOCALIB_REQ,      // Publish to set a new autocalib value for BACKLIGHT
    CONTRIB_REQ,        // Publish to set a new screen-emitted compensation value
    MSGS_SIZE
};

typedef struct {
    double lat;
    double lon;
} loc_t;

/** PubSub Messages **/

typedef struct {
    enum mod_msg_types type; /* LOCATION_UPD/LOCATION_REQ */ 
    loc_t old;
    loc_t new;
} loc_upd;

typedef struct {
    enum mod_msg_types type; /* UPOWER_UPD/UPOWER_REQ */
    enum ac_states old;
    enum ac_states new;
} upower_upd;

typedef struct {
    enum mod_msg_types type; /* INHIBIT_UPD/INHIBIT_REQ */
    bool old;
    bool new;
} inhibit_upd;

typedef struct {
    enum mod_msg_types type; /* DISPLAY_UPD */
    enum display_states old;
    enum display_states new;
} display_upd;

typedef struct {
    enum mod_msg_types type; /* TIME_UPD/EVENT_UPD */
    enum states old;
    enum states new;
} time_upd;

typedef struct {
    enum mod_msg_types type; /* SUNRISE_UPD/SUNSET_UPD */
    enum events old;
    enum events new;
} evt_upd;

typedef struct {
    enum mod_msg_types type; /* TEMP_UPD/TEMP_REQ */
    int old;
    int new;
    int smooth;              // Only useful for requests. -1 to use conf values
    int step;                // Only useful for requests
    int timeout;             // Only useful for requests
} temp_upd;

typedef struct {
    enum mod_msg_types type; /* DIMMER_TO_REQ/DPMS_TO_REQ/SCR_TO_REQ/BL_TO_REQ */
    int old;
    int new;
    enum ac_states state;
    enum states daytime;    // only useful for backlight timeouts!
} timeout_upd;

typedef struct {
    enum mod_msg_types type; /* CAPTURE_REQ */
} capture_upd;

typedef struct {
    enum mod_msg_types type; /* CURVE_REQ */
    enum ac_states state;
    double regression_points[MAX_SIZE_POINTS];
    int num_points;
} curve_upd;

typedef struct {
    enum mod_msg_types type; /* AUTOCALIB_REQ */
    int old;
    int new;
} calib_upd;

typedef struct {
    enum mod_msg_types type; /* AMBIENT_BR_UPD/BL_UPD/KBD_BL_UPD/SCR_BL_UPD/BL_REQ/KBD_BL_REQ */
    double old;
    double new;
    int smooth;              // Only useful for requests. -1 to use conf values
    double step;             // Only useful for requests
    int timeout;             // Only useful for requests
} bl_upd;

typedef struct {
    enum mod_msg_types type; /* CONTRIB_REQ */
    double old;
    double new;
} contrib_upd;

/** PubSub Topics **/
extern const char *topics[MSGS_SIZE];

/** Log function declaration **/

void log_message(const char *filename, int lineno, const char type, const char *log_msg, ...);
