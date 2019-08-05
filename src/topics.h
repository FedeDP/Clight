#pragma once


/* INHIBIT topics */
extern const char *inh_topic;

/* UPOWER topics */
extern const char *up_topic;

/* LOCATION topics */
extern const char *loc_topic;

/* BACKLIGHT topics */
extern const char *current_bl_topic;
extern const char *current_kbd_topic;
extern const char *current_ab_topic;

/* DIMMER/DPMS topics */
extern const char *display_topic;                   // defined in dpms module

/* INTERFACE topics */
extern const char *interface_temp_topic;
extern const char *interface_dimmer_to_topic;
extern const char *interface_dpms_to_topic;
extern const char *interface_bl_to_topic;
extern const char *interface_bl_capture;
extern const char *interface_bl_curve;
extern const char *interface_bl_autocalib;

/* GAMMA topics */
extern const char *time_topic;
extern const char *evt_topic;
extern const char *sunrise_topic;
extern const char *sunset_topic;
extern const char *temp_topic;
