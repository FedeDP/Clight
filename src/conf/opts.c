#include <popt.h>
#include "opts.h"

static void init_backlight_opts(bl_conf_t *bl_conf);
static void init_sens_opts(sensor_conf_t *sens_conf);
static void init_gamma_opts(gamma_conf_t *gamma_conf);
static void init_dimmer_opts(dimmer_conf_t *dim_conf);
static void init_dpms_opts(dpms_conf_t *dpms_conf);
static void init_screen_opts(screen_conf_t *screen_conf);
static void parse_cmd(int argc, char *const argv[], char *conf_file, size_t size);
static int parse_bus_reply(sd_bus_message *reply, const char *member, void *userdata);
static void check_clightd_features(void);
static void check_conf(void);

static void init_backlight_opts(bl_conf_t *bl_conf) {
    bl_conf->timeout[ON_AC][DAY] = 10 * 60;
    bl_conf->timeout[ON_AC][NIGHT] = 45 * 60;
    bl_conf->timeout[ON_AC][IN_EVENT] = 5 * 60;
    bl_conf->timeout[ON_BATTERY][DAY] = 2 * conf.bl_conf.timeout[ON_AC][DAY];
    bl_conf->timeout[ON_BATTERY][NIGHT] = 2 * conf.bl_conf.timeout[ON_AC][NIGHT];
    bl_conf->timeout[ON_BATTERY][IN_EVENT] = 2 * conf.bl_conf.timeout[ON_AC][IN_EVENT];
    bl_conf->backlight_trans_step = 0.05;
    bl_conf->backlight_trans_timeout = 30;
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
    sens_conf->num_points[ON_AC] = DEF_SIZE_POINTS;
    sens_conf->num_points[ON_BATTERY] = DEF_SIZE_POINTS;
    memcpy(sens_conf->regression_points[ON_AC],
           (double[]){ 0.0, 0.15, 0.29, 0.45, 0.61, 0.74, 0.81, 0.88, 0.93, 0.97, 1.0 },
           DEF_SIZE_POINTS * sizeof(double));
    memcpy(sens_conf->regression_points[ON_BATTERY],
           (double[]){ 0.0, 0.15, 0.23, 0.36, 0.52, 0.59, 0.65, 0.71, 0.75, 0.78, 0.80 },
           DEF_SIZE_POINTS * sizeof(double));
}

static void init_gamma_opts(gamma_conf_t *gamma_conf) {
    gamma_conf->temp[DAY] = 6500;
    gamma_conf->temp[NIGHT] = 4000;
    gamma_conf->event_duration = 30 * 60;
    gamma_conf->gamma_trans_step = 50;
    gamma_conf->gamma_trans_timeout = 300;
    gamma_conf->loc.lat = LAT_UNDEFINED;
    gamma_conf->loc.lon = LON_UNDEFINED;
}

static void init_dimmer_opts(dimmer_conf_t *dim_conf) {
    dim_conf->dimmer_timeout[ON_AC] = 45;
    dim_conf->dimmer_timeout[ON_BATTERY] = 20;
    dim_conf->dimmer_pct = 0.2;
    dim_conf->dimmer_trans_step[ENTER] = 0.05;
    dim_conf->dimmer_trans_step[EXIT] = 0.05;
    dim_conf->dimmer_trans_timeout[ENTER] = 30;
    dim_conf->dimmer_trans_timeout[EXIT] = 30;
}

static void init_dpms_opts(dpms_conf_t *dpms_conf) {
    dpms_conf->dpms_timeout[ON_AC] = 900;
    dpms_conf->dpms_timeout[ON_BATTERY] = 300;
}

static void init_screen_opts(screen_conf_t *screen_conf) {
    screen_conf->screen_timeout[ON_AC] = 30;
    screen_conf->screen_timeout[ON_BATTERY] = -1; // disabled on battery by default
    screen_conf->screen_contrib = 0.1;
    screen_conf->screen_samples = 10;
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
    init_gamma_opts(&conf.gamma_conf);
    init_dimmer_opts(&conf.dim_conf);
    init_dpms_opts(&conf.dpms_conf);
    init_screen_opts(&conf.screen_conf);

    char conf_file[PATH_MAX + 1] = {0};
    
    read_config(GLOBAL, conf_file);
    conf_file[0] = 0;
    read_config(LOCAL, conf_file);
    conf_file[0] = 0;
    parse_cmd(argc, argv, conf_file, PATH_MAX);
    
    /* --conf-file option was passed! */
    if (strlen(conf_file)) {
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
        {"frames", 'f', POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, NULL, 7, "Frames taken for each capture, Between 1 and 20", NULL},
        {"device", 'd', POPT_ARG_STRING, NULL, 1, "Path to webcam device. If empty, first matching device is used", "video0"},
        {"backlight", 'b', POPT_ARG_STRING, NULL, 2, "Path to backlight syspath. If empty, first matching device is used", "intel_backlight"},
        {"no-backlight-smooth", 0, POPT_ARG_NONE, &conf.bl_conf.no_smooth_backlight, 100, "Disable smooth backlight transitions", NULL},
        {"no-gamma-smooth", 0, POPT_ARG_NONE, &conf.gamma_conf.no_smooth_gamma, 100, "Disable smooth gamma transitions", NULL},
        {"no-dimmer-smooth-enter", 0, POPT_ARG_NONE, &conf.dim_conf.no_smooth_dimmer[ENTER], 100, "Disable smooth dimmer transitions while entering dimmed state", NULL},
        {"no-dimmer-smooth-exit", 0, POPT_ARG_NONE, &conf.dim_conf.no_smooth_dimmer[EXIT], 100, "Disable smooth dimmer transitions while leaving dimmed state", NULL},
        {"day-temp", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.gamma_conf.temp[DAY], 100, "Daily gamma temperature, between 1000 and 10000", NULL},
        {"night-temp", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.gamma_conf.temp[NIGHT], 100, "Nightly gamma temperature, between 1000 and 10000", NULL},
        {"lat", 0, POPT_ARG_DOUBLE, &conf.gamma_conf.loc.lat, 100, "Your desired latitude", NULL},
        {"lon", 0, POPT_ARG_DOUBLE, &conf.gamma_conf.loc.lon, 100, "Your desired longitude", NULL},
        {"sunrise", 0, POPT_ARG_STRING, NULL, 3, "Force sunrise time for gamma correction", "07:00"},
        {"sunset", 0, POPT_ARG_STRING, NULL, 4, "Force sunset time for gamma correction", "19:00"},
        {"no-gamma", 0, POPT_ARG_NONE, &conf.gamma_conf.no_gamma, 100, "Disable gamma correction tool", NULL},
        {"no-dimmer", 0, POPT_ARG_NONE, &conf.dim_conf.no_dimmer, 100, "Disable dimmer tool", NULL},
        {"no-dpms", 0, POPT_ARG_NONE, &conf.dpms_conf.no_dpms, 100, "Disable dpms tool", NULL},
        {"no-backlight", 0, POPT_ARG_NONE, &conf.bl_conf.no_backlight, 100, "Disable backlight module", NULL},
        {"no-screen", 0, POPT_ARG_NONE, &conf.screen_conf.no_screen, 100, "Disable screen module", NULL},
        {"dimmer-pct", 0, POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &conf.dim_conf.dimmer_pct, 100, "Backlight level used while screen is dimmed, in pergentage", NULL},
        {"verbose", 0, POPT_ARG_NONE, &conf.verbose, 100, "Enable verbose mode", NULL},
        {"no-auto-calib", 0, POPT_ARG_NONE, &conf.bl_conf.no_auto_calib, 100, "Disable screen backlight automatic calibration", NULL},
        {"no-kbd-backlight", 0, POPT_ARG_NONE, &conf.bl_conf.no_keyboard_bl, 100, "Disable keyboard backlight calibration", NULL},
        {"shutter-thres", 0, POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &conf.bl_conf.shutter_threshold, 100, "Threshold to consider a capture as clogged", NULL},
        {"version", 'v', POPT_ARG_NONE, NULL, 5, "Show version info", NULL},
        {"conf-file", 'c', POPT_ARG_STRING, NULL, 6, "Specify a conf file to be parsed", NULL},
        {"gamma-long-transition", 0, POPT_ARG_NONE, &conf.gamma_conf.gamma_long_transition, 100, "Enable a very long smooth transition for gamma (redshift-like)", NULL },
        {"ambient-gamma", 0, POPT_ARG_NONE, &conf.gamma_conf.ambient_gamma, 100, "Enable screen temperature matching ambient brightness instead of time based.", NULL },
        POPT_AUTOHELP
        POPT_TABLEEND
    };

    pc = poptGetContext(NULL, argc, (const char **)argv, po, 0);
    int rc;
    while ((rc = poptGetNextOpt(pc)) > 0) {
        char *str = poptGetOptArg(pc);
        switch (rc) {
            case 1:
                strncpy(conf.sens_conf.dev_name, str, sizeof(conf.sens_conf.dev_name) - 1);
                break;
            case 2:
                strncpy(conf.bl_conf.screen_path, str, sizeof(conf.bl_conf.screen_path) - 1);
                break;
            case 3:
                strncpy(conf.gamma_conf.day_events[SUNRISE], str, sizeof(conf.gamma_conf.day_events[SUNRISE]) - 1);
                break;
            case 4:
                strncpy(conf.gamma_conf.day_events[SUNSET], str, sizeof(conf.gamma_conf.day_events[SUNSET]) - 1);
                break;
            case 5:
                printf("%s: C daemon utility to automagically adjust screen backlight to match ambient brightness.\n"
                        "* Current version: %s\n"
                        "* https://github.com/FedeDP/Clight\n"
                        "* Copyright (C) 2020  Federico Di Pierro <nierro92@gmail.com>\n", argv[0], VERSION);
                exit(EXIT_SUCCESS);
            case 6:
                strncpy(conf_file, str, size);
                break;
            case 7:
                conf.sens_conf.num_captures[ON_AC] = atoi(str);
                conf.sens_conf.num_captures[ON_BATTERY] = atoi(str);
                break;
            default:
                break;
        }
        if (str) {
            free(str);
        }
    }
    /*
     * poptGetNextOpt returns -1 when the final argument has been parsed
     * otherwise an error occured
     */
    if (rc != -1) {
        ERROR("%s\n", poptStrerror(rc));
    }
    poptFreeContext(pc);
}

static int parse_bus_reply(sd_bus_message *reply, UNUSED const char *member,  UNUSED void *userdata) {
    const char *service_list;
    int r = sd_bus_message_read(reply, "s", &service_list);
    if (r < 0) {
        WARN("Clightd service could not be introspected. Automatic modules detection won't work.\n");
    } else {
        /* Check only optional build time features */
        if (!conf.gamma_conf.no_gamma && !strstr(service_list, "<node name=\"Gamma\"/>")) {
            conf.gamma_conf.no_gamma = true;
            WARN("GAMMA forcefully disabled as Clightd was built without gamma support.\n");
        }
        
        if (!conf.screen_conf.no_screen && !strstr(service_list, "<node name=\"Screen\"/>")) {
            conf.screen_conf.no_screen = true;
            WARN("SCREEN forcefully disabled as Clightd was built without screen support.\n");
        }
        
        if (!conf.dpms_conf.no_dpms && !strstr(service_list, "<node name=\"Dpms\"/>")) {
            conf.dpms_conf.no_dpms = true;
            WARN("DPMS forcefully disabled as Clightd was built without dpms support.\n");
        }
    }
    return r;
}

static void check_clightd_features(void) {
    SYSBUS_ARG_REPLY(introspect_args, parse_bus_reply, NULL, CLIGHTD_SERVICE, "/org/clightd/clightd", "org.freedesktop.DBus.Introspectable", "Introspect");
    call(&introspect_args, NULL);
}

/*
 * It does all needed checks to correctly reset default values
 * in case of wrong options set.
 */
static void check_conf(void) {
    /* GAMMA and SCREEN require X */
    if (!state.display || !state.xauthority) {
        if (!conf.gamma_conf.no_gamma) {
            INFO("Disabling GAMMA on non-X environment.\n");
            conf.gamma_conf.no_gamma = true;
        }
        if (!conf.screen_conf.no_screen) {
            INFO("Disabling SCREEN on non-X environment.\n");
            conf.screen_conf.no_screen = true;
        }
    }
    
    /* DPMS does not work in wayland */
    if (state.wl_display && !conf.dpms_conf.no_dpms) {
        INFO("Disabling DPMS in wayland environment.\n");
        conf.dpms_conf.no_dpms = true;
    }
    
    /* Forcefully disable ambient gamma, DIMMER and SCREEN if BACKLIGHT is disabled */
    if (conf.bl_conf.no_backlight) {
        if (conf.gamma_conf.ambient_gamma) {
            INFO("Disabling ambient gamma as BACKLIGHT is disabled.\n");
            conf.gamma_conf.ambient_gamma = false;
        }
        
        if (!conf.dim_conf.no_dimmer) {
            INFO("Disabling DIMMER as BACKLIGHT is disabled.\n");
            conf.dim_conf.no_dimmer = true;
        }

        if (!conf.screen_conf.no_screen) {
            INFO("Disabling SCREEN as BACKLIGHT is disabled.\n");
            conf.screen_conf.no_screen = true;
        }
    }

    /* Disable any not built feature in Clightd */
    check_clightd_features();
    
    // TODO: split and check all sensors
//     if (conf.sens_conf.num_captures[ON_AC] < 1 || conf.sens_conf.num_captures[ON_AC] > 20) {
//         WARN("Wrong AC frames value. Resetting default value.\n");
//         conf.sens_conf.num_captures[ON_AC] = 5;
//     }
//     if (conf.sens_conf.num_captures[ON_BATTERY] < 1 || conf.sens_conf.num_captures[ON_BATTERY] > 20) {
//         WARN("Wrong BATT frames value. Resetting default value.\n");
//         conf.sens_conf.num_captures[ON_BATTERY] = 5;
//     }
    if (conf.gamma_conf.temp[DAY] < 1000 || conf.gamma_conf.temp[DAY] > 10000) {
        WARN("Wrong daily temp value. Resetting default value.\n");
        conf.gamma_conf.temp[DAY] = 6500;
    }
    if (conf.gamma_conf.temp[NIGHT] < 1000 || conf.gamma_conf.temp[NIGHT] > 10000) {
        WARN("Wrong nightly temp value. Resetting default value.\n");
        conf.gamma_conf.temp[NIGHT] = 4000;
    }
    if (conf.gamma_conf.event_duration <= 0) {
        WARN("Wrong event duration value. Resetting default value.\n");
        conf.gamma_conf.event_duration = 30 * 60;
    }
    if (conf.dim_conf.dimmer_pct > 1 || conf.dim_conf.dimmer_pct < 0) {
        WARN("Wrong dimmer backlight percentage value. Resetting default value.\n");
        conf.dim_conf.dimmer_pct = 0.2;
    }
    if (fabs(conf.gamma_conf.loc.lat) > 90.0f && conf.gamma_conf.loc.lat != LAT_UNDEFINED) {
        WARN("Wrong latitude value. Resetting default value.\n");
        conf.gamma_conf.loc.lat = LAT_UNDEFINED;
    }
    if (fabs(conf.gamma_conf.loc.lon) > 180.0f && conf.gamma_conf.loc.lon != LON_UNDEFINED) {
        WARN("Wrong longitude value. Resetting default value.\n");
        conf.gamma_conf.loc.lon = LON_UNDEFINED;
    }
    if (conf.bl_conf.backlight_trans_step <= 0.0f || conf.bl_conf.backlight_trans_step >= 1.0f) {
        WARN("Wrong backlight_trans_step value. Resetting default value.\n");
        conf.bl_conf.backlight_trans_step = 0.05;
    }
    if (conf.dim_conf.dimmer_trans_step[ENTER] <= 0.0f || conf.dim_conf.dimmer_trans_step[ENTER] >= 1.0f) {
        WARN("Wrong dimmer_trans_step[ENTER] value. Resetting default value.\n");
        conf.dim_conf.dimmer_trans_step[ENTER] = 0.05;
    }
    if (conf.dim_conf.dimmer_trans_step[EXIT] <= 0.0f || conf.dim_conf.dimmer_trans_step[EXIT] >= 1.0f) {
        WARN("Wrong dimmer_trans_step[EXIT] value. Resetting default value.\n");
        conf.dim_conf.dimmer_trans_step[EXIT] = 0.05;
    }
    if (conf.gamma_conf.gamma_trans_step <= 0) {
        WARN("Wrong gamma_trans_step value. Resetting default value.\n");
        conf.gamma_conf.gamma_trans_step = 50;
    }
    if (conf.bl_conf.backlight_trans_timeout <= 0) {
        WARN("Wrong backlight_trans_timeout value. Resetting default value.\n");
        conf.bl_conf.backlight_trans_timeout = 30;
    }
    if (conf.dim_conf.dimmer_trans_timeout[ENTER] <= 0) {
        WARN("Wrong dimmer_trans_timeout[ENTER] value. Resetting default value.\n");
        conf.dim_conf.dimmer_trans_timeout[ENTER] = 30;
    }
    if (conf.dim_conf.dimmer_trans_timeout[EXIT] <= 0) {
        WARN("Wrong dimmer_trans_timeout[EXIT] value. Resetting default value.\n");
        conf.dim_conf.dimmer_trans_timeout[EXIT] = 30;
    }
    if (conf.gamma_conf.gamma_trans_timeout <= 0) {
        WARN("Wrong gamma_trans_timeout value. Resetting default value.\n");
        conf.gamma_conf.gamma_trans_timeout = 300;
    }
    if (conf.bl_conf.shutter_threshold < 0 || conf.bl_conf.shutter_threshold >= 1) {
        WARN("Wrong shutter_threshold value. Resetting default value.\n");
        conf.bl_conf.shutter_threshold = 0.0;
    }
    
    if (conf.dpms_conf.dpms_timeout[ON_AC] <= conf.dim_conf.dimmer_timeout[ON_AC]) {
        WARN("DPMS AC timeout: wrong value (<= dimmer timeout). Resetting default value.\n");
        conf.dpms_conf.dpms_timeout[ON_AC] = 900;
    }
    
    if (conf.dpms_conf.dpms_timeout[ON_BATTERY] <= conf.dim_conf.dimmer_timeout[ON_BATTERY]) {
        WARN("DPMS BATT timeout: wrong value (<= dimmer timeout). Resetting default value.\n");
        conf.dpms_conf.dpms_timeout[ON_BATTERY] = 300;
    }

    int i, reg_points_ac_needed = 0, reg_points_batt_needed = 0;
    /* Check regression points values */
    for (i = 0; i < conf.sens_conf.num_points[ON_AC] && !reg_points_ac_needed; i++) {
        if (conf.sens_conf.regression_points[ON_AC][i] < 0.0 || conf.sens_conf.regression_points[ON_AC][i] > 1.0) {
            reg_points_ac_needed = 1;
        }
    }
    for (i = 0; i < conf.sens_conf.num_points[ON_BATTERY] && !reg_points_batt_needed; i++) {
        if (conf.sens_conf.regression_points[ON_BATTERY][i] < 0.0 || conf.sens_conf.regression_points[ON_BATTERY][i] > 1.0) {
            reg_points_batt_needed = 1;
        }
    }
    
    if (reg_points_ac_needed) {
        WARN("Wrong ac_regression points. Resetting default values.\n");
        conf.sens_conf.num_points[ON_AC] = DEF_SIZE_POINTS;
        memcpy(conf.sens_conf.regression_points[ON_AC],
               (double[]){ 0.0, 0.15, 0.29, 0.45, 0.61, 0.74, 0.81, 0.88, 0.93, 0.97, 1.0 },
               DEF_SIZE_POINTS * sizeof(double));
    }
    if (reg_points_batt_needed) {
        WARN("Wrong batt_regression points. Resetting default values.\n");
        conf.sens_conf.num_points[ON_BATTERY] = DEF_SIZE_POINTS;
        memcpy(conf.sens_conf.regression_points[ON_BATTERY],
               (double[]){ 0.0, 0.15, 0.23, 0.36, 0.52, 0.59, 0.65, 0.71, 0.75, 0.78, 0.80 },
               DEF_SIZE_POINTS * sizeof(double));
    }

    for (i = 0; i < SIZE_EVENTS; i++) {
        struct tm timeinfo;
        if (strlen(conf.gamma_conf.day_events[i]) && !strptime(conf.gamma_conf.day_events[i], "%R", &timeinfo)) {
            memset(conf.gamma_conf.day_events[i], 0, sizeof(conf.gamma_conf.day_events[i]));
        }
    }
    
    if (conf.screen_conf.screen_contrib < 0 || conf.screen_conf.screen_contrib >= 1) {
        WARN("Wrong screen_contrib value. Resetting default value.\n");
        conf.screen_conf.screen_contrib = 0.1;
    }
    
    if (conf.screen_conf.screen_samples < 0) {
        WARN("Wrong screen_samples value. Resetting default value.\n");
        conf.screen_conf.screen_samples = 10;
    }
}
