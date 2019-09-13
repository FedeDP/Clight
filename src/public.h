#pragma once

#include <stddef.h>
#include <string.h>
#include <module/module_easy.h>

/** Libmodule wrapping macros, for convenience **/

#define CLIGHT_MODULE(name, ...)    MODULE(name); \
                                    static bool check(void) { return true; } \
                                    static bool evaluate(void) { return true; } \
                                    static void destroy(void) { } \
                                    extern int errno /* Declare errno to force a semicolon */

/** PubSub Macros **/

#define ASSERT_MSG(type);           _Static_assert(type >= 0 && type < MSGS_SIZE, "Wrong MSG type.");

#define MSG_TYPE()                  msg->is_pubsub ? (msg->ps_msg->type == USER ? ((message_t *)msg->ps_msg->message)->type : SYSTEM_UPD) : FD_UPD
#define MSG_DATA()                  ((uint8_t *)msg->ps_msg->message + offsetof(message_t, loc)) // offsetof any of the internal data structure to actually account for padding

#define DECLARE_MSG(name, type)     ASSERT_MSG(type); static message_t name = { type }

#define M_PUB(ptr)                  m_publish(topics[(ptr)->type], ptr, sizeof(message_t), false);
#define M_SUB(type)                 ASSERT_MSG(type); m_subscribe(topics[type]);

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
enum day_states { DAY, NIGHT, SIZE_STATES };

/* List of events: sunrise and sunset */
enum day_events { SUNRISE, SUNSET, SIZE_EVENTS };

/* Whether laptop is on battery or connected to ac */
enum ac_states { ON_AC, ON_BATTERY, SIZE_AC };

/* Display states */
enum display_states { DISPLAY_ON, DISPLAY_DIMMED, DISPLAY_OFF, DISPLAY_SIZE };

/* Quit values */
enum quit_values { NO_QUIT, NORM_QUIT, ERR_QUIT };

/* Dimming transition states */
enum dim_trans { ENTER, EXIT, SIZE_DIM };

/* Type of pubsub messages */

/* You should only subscribe on _UPD, and publish on _REQ */
enum mod_msg_types {
    SYSTEM_UPD = -2,    // Used internally by Clight
    FD_UPD = -1,        // Used internally by Clight
    LOC_UPD,            // Subscribe to receive new locations
    UPOWER_UPD,         // Subscribe to receive new AC states
    INHIBIT_UPD,        // Subscribe to receive new PowerManagement states
    DISPLAY_UPD,        // Subscribe to receive new display states (on/dimmed/off)
    DAYTIME_UPD,        // Subscribe to receive new daytime states (day/night)
    IN_EVENT_UPD,       // Subscribe to receive new InEvent states
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
    DISPLAY_REQ,        // Publish to set a new Display state (Set ~DISPLAY_DIMMED/~DISPLAY_OFF to unrequest a UNDIMMED/UN-DPMS state)
    SUNRISE_REQ,        // Publish to set a new fixed Sunrise
    SUNSET_REQ,         // Publish to set a new fixed Sunset
    TEMP_REQ,           // Publish to set a new gamma temp
    BL_REQ,             // Publish to set a new backlight level
    KBD_BL_REQ,         // Publish to set a new keyboard backlight level
    DIMMER_TO_REQ,      // Publish to set a new dimmer timeout
    DPMS_TO_REQ,        // Publish to set a new dpms timeout
    SCR_TO_REQ,         // Publish to set a new screen timeout
    BL_TO_REQ,          // Publish to set a new backlight timeout
    CAPTURE_REQ,        // Publish to set a new backlight calibration
    CURVE_REQ,          // Publish to set a new backlight curve for given ac state
    NO_AUTOCALIB_REQ,   // Publish to set a new no_autocalib value for BACKLIGHT
    CONTRIB_REQ,        // Publish to set a new screen-emitted compensation value
    MSGS_SIZE
};

typedef struct {
    double lat;
    double lon;
} loc_t;

/** PubSub Messages **/

typedef struct {
    loc_t old;                  // Valued in updates. Useless for requests
    loc_t new;                  // Mandatory for requests. Valued in updates
} loc_upd;

typedef struct {
    enum ac_states old;         // Valued in updates. Useless for requests
    enum ac_states new;         // Mandatory for requests. Valued in updates
} upower_upd;

typedef struct {
    bool old;                   // Valued in updates. Useless for requests 
    bool new;                   // Mandatory for requests. Valued in updates
} inhibit_upd;

typedef struct {
    enum display_states old;    // Valued in updates
    /*
     * Use DISPLAY_DIMMED to dim display.
     * Use DISPLAY_OFF to enter DPMS state.
     * Use DISPLAY_ON to restore normal state (ie: not DIMMED and not DPMS).
     */
    enum display_states new;    // Mandatory for requests. Valued in updates
} display_upd;

typedef struct {
    enum day_states old;        // Valued in updates
    enum day_states new;        // Valued in updates
} daytime_upd;

typedef struct {
    enum day_events old;        // Valued in updates. Useless for requests
    enum day_events new;        // Valued in updates. Useless for requests
    char event[10];             // Mandatory for requests. Empty on updates
} evt_upd;

typedef struct {
    enum day_states daytime;    // Mandatory for requests. Valued in updates
    int old;                    // Valued in updates. Useless for requests
    int new;                    // Mandatory for requests. Valued in updates
    int smooth;                 // Mandatory for requests. -1 to use conf values. Valued in updates
    int step;                   // Only useful for requests. Valued in updates
    int timeout;                // Only useful for requests. Valued in updates
} temp_upd;

typedef struct {
    int new;                    // Mandatory for requests
    enum ac_states state;       // Mandatory for requests
    enum day_states daytime;    // Mandatory for BL_TO_REQ only
} timeout_upd;

typedef struct {
    enum ac_states state;       // Mandatory for requests
    int num_points;             // Mandatory for requests
    double *regression_points;  // Mandatory for requests
} curve_upd;

typedef struct {
    bool new;                   // Mandatory for requests
} calib_upd;

typedef struct {
    bool reset_timer;           // Mandatory for requests. Whether to reset BACKLIGHT module internal capture timer after the capture
} capture_upd;

typedef struct {
    double old;                 // Valued in updates. Useless for requests
    double new;                 // Mandatory for requests. Valued in updates
    int smooth;                 // Mandatory for BL_REQ requests. -1 to use conf values. Valued in updates
    int timeout;                // Only useful for BL_REQ requests. Valued in updates
    double step;                // Only useful for BL_REQ requests. Valued in updates
} bl_upd;

typedef struct {
    double new;                 // Mandatory for requests
} contrib_upd;

typedef struct {
    const enum mod_msg_types type;
    union {
        loc_upd loc;            /* LOCATION_UPD/LOCATION_REQ */
        upower_upd upower;      /* UPOWER_UPD/UPOWER_REQ */
        inhibit_upd inhibit;    /* INHIBIT_UPD/INHIBIT_REQ */
        display_upd display;    /* DISPLAY_UPD/DISPLAY_REQ */
        daytime_upd day_time;   /* TIME_UPD/IN_EVENT_UPD */
        evt_upd event;          /* SUNRISE_UPD/SUNSET_UPD/SUNRISE_REQ/SUNSET_REQ */
        temp_upd temp;          /* TEMP_UPD/TEMP_REQ */
        timeout_upd to;         /* DIMMER_TO_REQ/DPMS_TO_REQ/SCR_TO_REQ/BL_TO_REQ */
        curve_upd curve;        /* CURVE_REQ */
        calib_upd nocalib;      /* NO_AUTOCALIB_REQ */
        bl_upd bl;              /* AMBIENT_BR_UPD/BL_UPD/KBD_BL_UPD/SCR_BL_UPD/BL_REQ/KBD_BL_REQ */
        contrib_upd contrib;    /* CONTRIB_REQ */
        capture_upd capture;    /* CAPTURE_REQ */
    };
} message_t;

/** PubSub Topics **/
extern const char *topics[];

/** Log function declaration **/

void log_message(const char *filename, int lineno, const char type, const char *log_msg, ...);
