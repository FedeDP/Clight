#include <popt.h>
#include "opts.h"
#include "utils.h"

static void init_backlight_opts(bl_conf_t *bl_conf);
static void init_sens_opts(sensor_conf_t *sens_conf);
static void init_override_opts(sensor_conf_t *sens_conf);
static void init_kbd_opts(kbd_conf_t *kbd_conf);
static void init_gamma_opts(gamma_conf_t *gamma_conf);
static void init_daytime_opts(daytime_conf_t *day_conf);
static void init_dimmer_opts(dimmer_conf_t *dim_conf);
static void init_dpms_opts(dpms_conf_t *dpms_conf);
static void init_screen_opts(screen_conf_t *screen_conf);
static void parse_cmd(int argc, char *const argv[], char *conf_file, size_t size);
static int parse_bus_reply(sd_bus_message *reply, const char *member, void *userdata);
static void check_clightd_features(void);
static void check_bl_conf(bl_conf_t *bl_conf);
static void check_sens_conf(sensor_conf_t *sens_conf);
static void check_override_conf(sensor_conf_t *sens_conf);
static void check_kbd_conf(kbd_conf_t *kbd_conf);
static void check_gamma_conf(gamma_conf_t *gamma_conf);
static void check_daytime_conf(daytime_conf_t *day_conf);
static void check_dim_conf(dimmer_conf_t *dim_conf);
static void check_dpms_conf(dpms_conf_t *dpms_conf);
static void check_screen_conf(screen_conf_t *screen_conf);
static void check_inh_conf(inh_conf_t *inh_conf);
static void check_conf(void);

static double *bl_default_curve[SIZE_AC] = { 
    (double[]){ 0.0, 0.15, 0.29, 0.45, 0.61, 0.74, 0.81, 0.88, 0.93, 0.97, 1.0 },
    (double[]){ 0.0, 0.15, 0.23, 0.36, 0.52, 0.59, 0.65, 0.71, 0.75, 0.78, 0.80 },
};

static double *kbd_default_curve[SIZE_AC] = { 
    (double[]){ 1.0, 0.97, 0.93, 0.88, 0.81, 0.74, 0.61, 0.45, 0.29, 0.15, 0.0 },
    (double[]){ 0.80, 0.78, 0.75, 0.71, 0.65, 0.59, 0.52, 0.36, 0.23, 0.15, 0.0 },
};

static void init_backlight_opts(bl_conf_t *bl_conf) {
    bl_conf->timeout[ON_AC][DAY] = 10 * 60;
    bl_conf->timeout[ON_AC][NIGHT] = 45 * 60;
    bl_conf->timeout[ON_AC][IN_EVENT] = 5 * 60;
    bl_conf->timeout[ON_BATTERY][DAY] = 2 * conf.bl_conf.timeout[ON_AC][DAY];
    bl_conf->timeout[ON_BATTERY][NIGHT] = 2 * conf.bl_conf.timeout[ON_AC][NIGHT];
    bl_conf->timeout[ON_BATTERY][IN_EVENT] = 2 * conf.bl_conf.timeout[ON_AC][IN_EVENT];
    bl_conf->smooth.trans_step = 0.05;
    bl_conf->smooth.trans_timeout = 30;
}

static void init_sens_opts(sensor_conf_t *sens_conf) {
    sens_conf->num_captures[ON_AC] = 5;
    sens_conf->num_captures[ON_BATTERY] = 5;
    /*
     * Default polynomial regression points:
     * ON AC                ON BATTERY
     * X = 0  Y = 0.00      X = 0  Y = 0.00
     *     1      0.15          1      0.15
     *     2      0.29          2      0.23
     *     3      0.45          3      0.36
     *     4      0.61          4      0.52
     *     5      0.74          5      0.59
     *     6      0.81          6      0.65
     *     7      0.88          7      0.71
     *     8      0.93          8      0.75
     *     9      0.97          9      0.78
     *    10      1.00         10      0.80
     * Where X is ambient brightness and Y is backlight level.
     * Empirically built (fast growing curve for lower values, and flattening m for values near 1)
     */
    sens_conf->default_curve[ON_AC].num_points = DEF_SIZE_POINTS;
    sens_conf->default_curve[ON_BATTERY].num_points = DEF_SIZE_POINTS;
    memcpy(sens_conf->default_curve[ON_AC].points,
           bl_default_curve[ON_AC],
           DEF_SIZE_POINTS * sizeof(double));
    memcpy(sens_conf->default_curve[ON_BATTERY].points,
           bl_default_curve[ON_BATTERY],
           DEF_SIZE_POINTS * sizeof(double));
}

static void init_override_opts(sensor_conf_t *sens_conf) {
    sens_conf->specific_curves = map_new(true, free);
}

static void init_kbd_opts(kbd_conf_t *kbd_conf) {
    kbd_conf->timeout[ON_AC] = 15;
    kbd_conf->timeout[ON_BATTERY] = 5;
    kbd_conf->curve[ON_AC].num_points = DEF_SIZE_POINTS;
    kbd_conf->curve[ON_BATTERY].num_points = DEF_SIZE_POINTS;
    memcpy(kbd_conf->curve[ON_AC].points,
           kbd_default_curve[ON_AC],
           DEF_SIZE_POINTS * sizeof(double));
    memcpy(kbd_conf->curve[ON_BATTERY].points,
           kbd_default_curve[ON_BATTERY],
           DEF_SIZE_POINTS * sizeof(double));
}

static void init_gamma_opts(gamma_conf_t *gamma_conf) {
    gamma_conf->temp[DAY] = 6500;
    gamma_conf->temp[NIGHT] = 4000;
    gamma_conf->trans_step = 50;
    gamma_conf->trans_timeout = 300;
}

static void init_daytime_opts(daytime_conf_t *day_conf) {
    day_conf->event_duration = 30 * 60;
    day_conf->loc.lat = LAT_UNDEFINED;
    day_conf->loc.lon = LON_UNDEFINED;
}

static void init_dimmer_opts(dimmer_conf_t *dim_conf) {
    dim_conf->timeout[ON_AC] = 45;
    dim_conf->timeout[ON_BATTERY] = 20;
    dim_conf->dimmed_pct = 0.2;
    dim_conf->smooth[ENTER].trans_step = 0.05;
    dim_conf->smooth[EXIT].trans_step = 0.05;
    dim_conf->smooth[ENTER].trans_timeout = 30;
    dim_conf->smooth[EXIT].trans_timeout = 30;
}

static void init_dpms_opts(dpms_conf_t *dpms_conf) {
    dpms_conf->timeout[ON_AC] = 900;
    dpms_conf->timeout[ON_BATTERY] = 300;
}

static void init_screen_opts(screen_conf_t *screen_conf) {
    screen_conf->contrib = 0.2;
    screen_conf->timeout[ON_AC] = 5;
    screen_conf->timeout[ON_BATTERY] = -1; // disabled on battery by default
}

/*
 * Init default config values,
 * parse both global and user-local config files through libconfig,
 * and parse cmdline args through popt lib.
 * Finally, check configuration values and log it.
 */
void init_opts(int argc, char *argv[]) {
    init_backlight_opts(&conf.bl_conf);
    init_sens_opts(&conf.sens_conf);
    init_override_opts(&conf.sens_conf);
    init_kbd_opts(&conf.kbd_conf);
    init_gamma_opts(&conf.gamma_conf);
    init_daytime_opts(&conf.day_conf);
    init_dimmer_opts(&conf.dim_conf);
    init_dpms_opts(&conf.dpms_conf);
    init_screen_opts(&conf.screen_conf);
    // init_inh_opts NOT NEEDED

    char conf_file[PATH_MAX + 1] = {0};
    
    read_config(GLOBAL, conf_file);
    conf_file[0] = 0;
    read_config(LOCAL, conf_file);
    conf_file[0] = 0;
    parse_cmd(argc, argv, conf_file, PATH_MAX);
    
    /* --conf-file option was passed! */
    if (!is_string_empty(conf_file)) {
        read_config(CUSTOM, conf_file);
    }
    
    check_conf();
}

/*
 * Parse cmdline to get cmd line options
 */
static void parse_cmd(int argc, char *const argv[], char *conf_file, size_t size) {
    poptContext pc;
    const struct poptOption po[] = {
        {"frames", 'f', POPT_ARG_INT, NULL, 5, "Frames taken for each capture, Between 1 and 20", NULL},
        {"device", 'd', POPT_ARG_STRING, &conf.sens_conf.dev_name, 100, "Path to sensor device. If empty, first matching device is used", "video0"},
        {"no-backlight-smooth", 0, POPT_ARG_NONE, &conf.bl_conf.smooth.no_smooth, 100, "Disable smooth backlight transitions", NULL},
        {"no-gamma-smooth", 0, POPT_ARG_NONE, &conf.gamma_conf.no_smooth, 100, "Disable smooth gamma transitions", NULL},
        {"no-dimmer-smooth-enter", 0, POPT_ARG_NONE, &conf.dim_conf.smooth[ENTER].no_smooth, 100, "Disable smooth dimmer transitions while entering dimmed state", NULL},
        {"no-dimmer-smooth-exit", 0, POPT_ARG_NONE, &conf.dim_conf.smooth[EXIT].no_smooth, 100, "Disable smooth dimmer transitions while leaving dimmed state", NULL},
        {"day-temp", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.gamma_conf.temp[DAY], 100, "Daily gamma temperature, between 1000 and 10000", NULL},
        {"night-temp", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.gamma_conf.temp[NIGHT], 100, "Nightly gamma temperature, between 1000 and 10000", NULL},
        {"lat", 0, POPT_ARG_DOUBLE, &conf.day_conf.loc.lat, 100, "Your desired latitude", NULL},
        {"lon", 0, POPT_ARG_DOUBLE, &conf.day_conf.loc.lon, 100, "Your desired longitude", NULL},
        {"sunrise", 0, POPT_ARG_STRING, NULL, 1, "Force sunrise time for gamma correction", "07:00"},
        {"sunset", 0, POPT_ARG_STRING, NULL, 2, "Force sunset time for gamma correction", "19:00"},
        {"no-gamma", 0, POPT_ARG_NONE, &conf.gamma_conf.disabled, 100, "Disable gamma correction tool", NULL},
        {"no-dimmer", 0, POPT_ARG_NONE, &conf.dim_conf.disabled, 100, "Disable dimmer tool", NULL},
        {"no-dpms", 0, POPT_ARG_NONE, &conf.dpms_conf.disabled, 100, "Disable dpms tool", NULL},
        {"no-backlight", 0, POPT_ARG_NONE, &conf.bl_conf.disabled, 100, "Disable backlight module", NULL},
        {"no-screen", 0, POPT_ARG_NONE, &conf.screen_conf.disabled, 100, "Disable screen module (screen content based backlight adjustment)", NULL},
        {"no-kbd", 0, POPT_ARG_NONE, &conf.kbd_conf.disabled, 100, "Disable keyboard backlight calibration", NULL},
        {"dimmer-pct", 0, POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &conf.dim_conf.dimmed_pct, 100, "Backlight level used while screen is dimmed, in pergentage", NULL},
        {"verbose", 0, POPT_ARG_NONE, &conf.verbose, 100, "Enable verbose mode", NULL},
        {"no-auto-calib", 0, POPT_ARG_NONE, &conf.bl_conf.no_auto_calib, 100, "Disable screen backlight automatic calibration", NULL},
        {"shutter-thres", 0, POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &conf.bl_conf.shutter_threshold, 100, "Threshold to consider a capture as clogged", NULL},
        {"version", 'v', POPT_ARG_NONE, NULL, 3, "Show version info", NULL},
        {"conf-file", 'c', POPT_ARG_STRING, NULL, 4, "Specify a conf file to be parsed", NULL},
        {"gamma-long-transition", 0, POPT_ARG_NONE, &conf.gamma_conf.long_transition, 100, "Enable a very long smooth transition for gamma (redshift-like)", NULL },
        {"ambient-gamma", 0, POPT_ARG_NONE, &conf.gamma_conf.ambient_gamma, 100, "Enable screen temperature matching ambient brightness instead of time based.", NULL },
        {"wizard", 'w', POPT_ARG_NONE, &conf.wizard, 100, "Enable wizard mode.", NULL},
        POPT_AUTOHELP
        POPT_TABLEEND
    };

    pc = poptGetContext(NULL, argc, (const char **)argv, po, 0);
    int rc;
    while ((rc = poptGetNextOpt(pc)) > 0) {
        char *str = poptGetOptArg(pc);
        switch (rc) {
            case 1:
                strncpy(conf.day_conf.day_events[SUNRISE], str, sizeof(conf.day_conf.day_events[SUNRISE]) - 1);
                break;
            case 2:
                strncpy(conf.day_conf.day_events[SUNSET], str, sizeof(conf.day_conf.day_events[SUNSET]) - 1);
                break;
            case 3:
                printf("%s: C daemon utility to automagically adjust screen backlight to match ambient brightness.\n"
                        "* Current version: %s\n"
                        "* https://github.com/FedeDP/Clight\n"
                        "* Copyright (C) 2022  Federico Di Pierro <nierro92@gmail.com>\n"
                        "* For more info, see man clight.1.\n", argv[0], VERSION);
                free(str);
                exit(EXIT_SUCCESS);
            case 4:
                strncpy(conf_file, str, size);
                break;
            case 5:
                conf.sens_conf.num_captures[ON_AC] = atoi(str);
                conf.sens_conf.num_captures[ON_BATTERY] = atoi(str);
                break;
            default:
                break;
        }
        free(str);
    }
    /*
     * poptGetNextOpt returns -1 when the final argument has been parsed
     * otherwise an error occurred
     */
    poptFreeContext(pc);
    if (rc != -1) {
        ERROR("%s\n", poptStrerror(rc));
    }
}

static int parse_bus_reply(sd_bus_message *reply, UNUSED const char *member,  UNUSED void *userdata) {
    if (!strcmp(member, "Ping")) {
        return 0;
    }
    const char *service_list;
    int r = sd_bus_message_read(reply, "s", &service_list);
    if (r < 0) {
        WARN("Clightd service could not be introspected. Automatic modules detection won't work.\n");
    } else {
        /* Check only optional build time features */
        if (!conf.gamma_conf.disabled && !strstr(service_list, "<node name=\"Gamma\"/>")) {
            conf.gamma_conf.disabled = true;
            WARN("GAMMA forcefully disabled as Clightd was built without gamma support.\n");
        }
        
        if (!conf.screen_conf.disabled && !strstr(service_list, "<node name=\"Screen\"/>")) {
            conf.screen_conf.disabled = true;
            WARN("SCREEN forcefully disabled as Clightd was built without screen support.\n");
        }
        
        if (!conf.dpms_conf.disabled && !strstr(service_list, "<node name=\"Dpms\"/>")) {
            conf.dpms_conf.disabled = true;
            WARN("DPMS forcefully disabled as Clightd was built without dpms support.\n");
        }
    }
    return r;
}

static void check_clightd_features(void) {
    /* 
     * Pass a non-null callback here, even if Ping method has no answer,
     * because we need to wait for clightd to get started, if it is not yet.
     */
    SYSBUS_ARG_REPLY(ping_args, parse_bus_reply, NULL, CLIGHTD_SERVICE, "/org/clightd/clightd", "org.freedesktop.DBus.Peer", "Ping");
    call(&ping_args, NULL);
    
    SYSBUS_ARG_REPLY(introspect_args, parse_bus_reply, NULL, CLIGHTD_SERVICE, "/org/clightd/clightd", "org.freedesktop.DBus.Introspectable", "Introspect");
    call(&introspect_args, NULL);
}

static void check_bl_conf(bl_conf_t *bl_conf) {
    if (bl_conf->smooth.trans_step <= 0.0f || bl_conf->smooth.trans_step >= 1.0f) {
        WARN("BL_CONF: wrong 'trans_step' value. Resetting default value.\n");
        bl_conf->smooth.trans_step = 0.05;
    }
    
    if (bl_conf->smooth.trans_timeout <= 0) {
        WARN("BL_CONF: wrong 'trans_timeout' value. Resetting default value.\n");
        bl_conf->smooth.trans_timeout = 30;
    }
    
    if (bl_conf->shutter_threshold < 0 || bl_conf->shutter_threshold >= 1) {
        WARN("BL_CONF: wrong 'shutter_threshold' value. Resetting default value.\n");
        bl_conf->shutter_threshold = 0.0;
    }
    
    if (bl_conf->sync_monitors_delay < 0 || bl_conf->sync_monitors_delay > 10) {
        WARN("BL_CONF: wrong 'hotplug_delay' value. Resetting default value.\n");
        bl_conf->sync_monitors_delay = 0;
    }
}

static inline void check_curve_points(curve_t *c, const char *prefix, const char *id, double *fallback[SIZE_AC]) {
    int i, reg_points_ac_needed = 0, reg_points_batt_needed = 0;
    /* Check regression points values */
    for (i = 0; i < c[ON_AC].num_points && !reg_points_ac_needed; i++) {
        if (c[ON_AC].points[i] < 0.0 || c[ON_AC].points[i] > 1.0) {
            reg_points_ac_needed = 1;
        }
    }
    for (i = 0; i < c[ON_BATTERY].num_points && !reg_points_batt_needed; i++) {
        if (c[ON_BATTERY].points[i] < 0.0 || c[ON_BATTERY].points[i] > 1.0) {
            reg_points_batt_needed = 1;
        }
    }
    if (reg_points_ac_needed) {
        WARN("%s: wrong %s 'ac_regression' points. Resetting default values.\n", prefix, id);
        c[ON_AC].num_points = DEF_SIZE_POINTS;
        memcpy(c[ON_AC].points, fallback[ON_AC], DEF_SIZE_POINTS * sizeof(double));
    }
    if (reg_points_batt_needed) {
        WARN("%s: wrong %s 'batt_regression' points. Resetting default values.\n", prefix, id);
        c[ON_BATTERY].num_points = DEF_SIZE_POINTS;
        memcpy(c[ON_BATTERY].points, fallback[ON_BATTERY], DEF_SIZE_POINTS * sizeof(double));
    }
}

static void check_sens_conf(sensor_conf_t *sens_conf) {
    if (sens_conf->num_captures[ON_AC] < 1 || sens_conf->num_captures[ON_AC] > 20) {
        WARN("SENS_CONF: wrong AC 'captures' value. Resetting default value.\n");
        sens_conf->num_captures[ON_AC] = 5;
    }
    if (sens_conf->num_captures[ON_BATTERY] < 1 || sens_conf->num_captures[ON_BATTERY] > 20) {
        WARN("SENS_CONF: wrong BATT 'captures' value. Resetting default value.\n");
        sens_conf->num_captures[ON_BATTERY] = 5;
    }
    check_curve_points(sens_conf->default_curve, "SENS_CONF", "sensor", bl_default_curve);
}

static void check_override_conf(sensor_conf_t *sens_conf) {
    for (map_itr_t *itr = map_itr_new(sens_conf->specific_curves); itr; itr = map_itr_next(itr)) {
        curve_t *c = map_itr_get_data(itr);
        const char *id = map_itr_get_key(itr);
        check_curve_points(c, "MON_CONF", id, bl_default_curve);
    }
}

static void check_kbd_conf(kbd_conf_t *kbd_conf) {
    check_curve_points(kbd_conf->curve, "KBD_CONF", "keyboard", kbd_default_curve);
}

static void check_gamma_conf(gamma_conf_t *gamma_conf) {
    if (conf.bl_conf.disabled && gamma_conf->ambient_gamma) {
        INFO("GAMMA_CONF: disabling 'ambient_gamma' as BACKLIGHT is disabled.\n");
        gamma_conf->ambient_gamma = false;
    }
    
    if (gamma_conf->temp[DAY] < 1000 || gamma_conf->temp[DAY] > 10000) {
        WARN("GAMMA_CONF: wrong daily 'temp' value. Resetting default value.\n");
        gamma_conf->temp[DAY] = 6500;
    }
    if (gamma_conf->temp[NIGHT] < 1000 || gamma_conf->temp[NIGHT] > 10000) {
        WARN("GAMMA_CONF: wrong nightly 'temp' value. Resetting default value.\n");
        gamma_conf->temp[NIGHT] = 4000;
    }
    
    if (gamma_conf->trans_step <= 0) {
        WARN("GAMMA_CONF: wrong 'trans_step' value. Resetting default value.\n");
        gamma_conf->trans_step = 50;
    }
    if (gamma_conf->trans_timeout <= 0) {
        WARN("GAMMA_CONF: wrong 'trans_timeout' value. Resetting default value.\n");
        gamma_conf->trans_timeout = 300;
    }
}

static void check_daytime_conf(daytime_conf_t *day_conf) {
    if (day_conf->event_duration <= 0) {
        WARN("DAYTIME_CONF: wrong 'event_duration' value. Resetting default value.\n");
        day_conf->event_duration = 30 * 60;
    }
    
    if (fabs(day_conf->loc.lat) > 90.0f && day_conf->loc.lat != LAT_UNDEFINED) {
        WARN("DAYTIME_CONF: wrong 'latitude' value. Resetting default value.\n");
        day_conf->loc.lat = LAT_UNDEFINED;
    }
    if (fabs(day_conf->loc.lon) > 180.0f && day_conf->loc.lon != LON_UNDEFINED) {
        WARN("DAYTIME_CONF: wrong 'longitude' value. Resetting default value.\n");
        day_conf->loc.lon = LON_UNDEFINED;
    }
    
    for (int i = 0; i < SIZE_EVENTS; i++) {
        struct tm timeinfo;
        if (!is_string_empty(day_conf->day_events[i]) && 
            !strptime(day_conf->day_events[i], "%R", &timeinfo)) {
            
            memset(day_conf->day_events[i], 0, sizeof(day_conf->day_events[i]));
        }
    }
}

static void check_dim_conf(dimmer_conf_t *dim_conf) {
    if (conf.bl_conf.disabled) {
        INFO("DIMMER_CONF: disabling as BACKLIGHT is disabled.\n");
        dim_conf->disabled = true;
    }
    
    if (dim_conf->dimmed_pct > 1 || dim_conf->dimmed_pct < 0) {
        WARN("DIMMER_CONF: wrong 'dimmed_pct' value. Resetting default value.\n");
        dim_conf->dimmed_pct = 0.2;
    }
    
    if (dim_conf->smooth[ENTER].trans_step <= 0.0f || dim_conf->smooth[ENTER].trans_step >= 1.0f) {
        WARN("DIMMER_CONF: wrong 'trans_step[ENTER]' value. Resetting default value.\n");
        dim_conf->smooth[ENTER].trans_step = 0.05;
    }
    if (dim_conf->smooth[EXIT].trans_step <= 0.0f || dim_conf->smooth[EXIT].trans_step >= 1.0f) {
        WARN("DIMMER_CONF: wrong 'trans_step[EXIT]' value. Resetting default value.\n");
        dim_conf->smooth[EXIT].trans_step = 0.05;
    }
    
    if (dim_conf->smooth[ENTER].trans_timeout <= 0) {
        WARN("DIMMER_CONF: wrong 'trans_timeout[ENTER]' value. Resetting default value.\n");
        dim_conf->smooth[ENTER].trans_timeout = 30;
    }
    if (dim_conf->smooth[EXIT].trans_timeout <= 0) {
        WARN("DIMMER_CONF: wrong 'trans_timeout[EXIT]' value. Resetting default value.\n");
        dim_conf->smooth[EXIT].trans_timeout = 30;
    }
}

static void check_dpms_conf(dpms_conf_t *dpms_conf) {
    if (!conf.dim_conf.disabled) {
        if (dpms_conf->timeout[ON_AC] <= conf.dim_conf.timeout[ON_AC]) {
            WARN("DPMS_CONF: wrong AC 'timeout' value (<= dimmer timeout). Resetting default value.\n");
            dpms_conf->timeout[ON_AC] = 900;
        }
        
        if (dpms_conf->timeout[ON_BATTERY] <= conf.dim_conf.timeout[ON_BATTERY]) {
            WARN("DPMS_CONF: wrong BATT 'timeout' value (<= dimmer timeout). Resetting default value.\n");
            dpms_conf->timeout[ON_BATTERY] = 300;
        }
    }
}

static void check_screen_conf(screen_conf_t *screen_conf) {
    if (conf.bl_conf.disabled) {
        INFO("SCREEN_CONF: disabling as BACKLIGHT is disabled.\n");
        screen_conf->disabled = true;
    }
    
    if (screen_conf->contrib < 0.0f || screen_conf->contrib > 1.0f) {
        WARN("SCREEN_CONF: wrong 'contrib' value. Resetting default value.\n");
        screen_conf->contrib = 0.2;
    }
}

static void check_inh_conf(inh_conf_t *inh_conf) {
    if (conf.dim_conf.disabled && conf.dpms_conf.disabled) {
        DEBUG("INHIBIT_CONF: not needed. Disabling.\n");
        inh_conf->disabled = true;
        inh_conf->inhibit_docked = false;
        inh_conf->inhibit_pm = false;
    }
}

/*
 * It does all needed checks to correctly reset default values
 * in case of wrong options set.
 */
static void check_conf(void) {
    /* Wizard mode; disable everything except backlight */
    if (conf.wizard) {
        conf.bl_conf.no_auto_calib = true;
        conf.kbd_conf.disabled = true;
        conf.gamma_conf.disabled = true;
        conf.dim_conf.disabled = true;
        conf.dpms_conf.disabled = true;
        conf.screen_conf.disabled = true;
    } else {
        /* Disable any not built-in feature in Clightd */
        check_clightd_features();
    }
    
    if (conf.resumedelay < 0 || conf.resumedelay > 30) {
        WARN("CONF: wrong 'resumedelay' value. Resetting default value.\n");
        conf.resumedelay = 0;
    }
    
    if (!conf.bl_conf.disabled) {
        check_bl_conf(&conf.bl_conf);
        check_sens_conf(&conf.sens_conf);
        check_override_conf(&conf.sens_conf);
    }
    if (!conf.kbd_conf.disabled) {
        check_kbd_conf(&conf.kbd_conf);
    }
    if (!conf.gamma_conf.disabled) {
        check_gamma_conf(&conf.gamma_conf);
    }
    check_daytime_conf(&conf.day_conf);
    if (!conf.dim_conf.disabled) {
        check_dim_conf(&conf.dim_conf);
    }
    if (!conf.dpms_conf.disabled) {
        check_dpms_conf(&conf.dpms_conf);
    }
    if (!conf.screen_conf.disabled) {
        check_screen_conf(&conf.screen_conf);
    }
    check_inh_conf(&conf.inh_conf);
}
