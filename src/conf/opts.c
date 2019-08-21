#include <opts.h>
#include <popt.h>

static void parse_cmd(int argc, char *const argv[], char *conf_file, size_t size);
static void check_conf(void);

/*
 * Init default config values,
 * parse both global and user-local config files through libconfig,
 * and parse cmdline args through popt lib.
 * Finally, check configuration values and log it.
 */
void init_opts(int argc, char *argv[]) {    
    /* BACKLIGHT */
    conf.num_captures = 5;
    conf.timeout[ON_AC][DAY] = 10 * 60;
    conf.timeout[ON_AC][NIGHT] = 45 * 60;
    conf.timeout[ON_AC][IN_EVENT] = 5 * 60;
    conf.timeout[ON_BATTERY][DAY] = 2 * conf.timeout[ON_AC][DAY];
    conf.timeout[ON_BATTERY][NIGHT] = 2 * conf.timeout[ON_AC][NIGHT];
    conf.timeout[ON_BATTERY][IN_EVENT] = 2 * conf.timeout[ON_AC][IN_EVENT];
    conf.backlight_trans_step = 0.05;
    conf.backlight_trans_timeout = 30;
    
    /* GAMMA */
    conf.temp[DAY] = 6500;
    conf.temp[NIGHT] = 4000;
    conf.event_duration = 30 * 60;
    conf.gamma_trans_step = 50;
    conf.gamma_trans_timeout = 300;
    
    /* DIMMER */
    conf.dimmer_timeout[ON_AC] = 45;
    conf.dimmer_timeout[ON_BATTERY] = 20;
    conf.dimmer_pct = 0.2;
    conf.dimmer_trans_step[ENTER] = 0.05;
    conf.dimmer_trans_step[EXIT] = 0.05;
    conf.dimmer_trans_timeout[ENTER] = 30;
    conf.dimmer_trans_timeout[EXIT] = 30;
    
    /* DPMS */
    conf.dpms_timeout[ON_AC] = 900;
    conf.dpms_timeout[ON_BATTERY] = 300;

    /* SCREEN */
    conf.screen_timeout[ON_AC] = 30;
    conf.screen_timeout[ON_BATTERY] = -1; // disabled on battery by default
    conf.screen_contrib = 0.1;
    conf.screen_samples = 10;
    
    /* LOCATION */
    conf.loc.lat = LAT_UNDEFINED;
    conf.loc.lon = LON_UNDEFINED;

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
    conf.num_points[ON_AC] = DEF_SIZE_POINTS;
    conf.num_points[ON_BATTERY] = DEF_SIZE_POINTS;
    memcpy(conf.regression_points[ON_AC],
           (double[]){ 0.0, 0.15, 0.29, 0.45, 0.61, 0.74, 0.81, 0.88, 0.93, 0.97, 1.0 },
           DEF_SIZE_POINTS * sizeof(double));
    memcpy(conf.regression_points[ON_BATTERY],
           (double[]){ 0.0, 0.15, 0.23, 0.36, 0.52, 0.59, 0.65, 0.71, 0.75, 0.78, 0.80 },
           DEF_SIZE_POINTS * sizeof(double));

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
        {"frames", 'f', POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.num_captures, 100, "Frames taken for each capture, Between 1 and 20", NULL},
        {"device", 'd', POPT_ARG_STRING, NULL, 1, "Path to webcam device. If empty, first matching device is used", "video0"},
        {"backlight", 'b', POPT_ARG_STRING, NULL, 2, "Path to backlight syspath. If empty, first matching device is used", "intel_backlight"},
        {"no-backlight-smooth", 0, POPT_ARG_NONE, &conf.no_smooth_backlight, 100, "Disable smooth backlight transitions", NULL},
        {"no-gamma-smooth", 0, POPT_ARG_NONE, &conf.no_smooth_gamma, 100, "Disable smooth gamma transitions", NULL},
        {"no-dimmer-smooth-enter", 0, POPT_ARG_NONE, &conf.no_smooth_dimmer[ENTER], 100, "Disable smooth dimmer transitions while entering dimmed state", NULL},
        {"no-dimmer-smooth-exit", 0, POPT_ARG_NONE, &conf.no_smooth_dimmer[EXIT], 100, "Disable smooth dimmer transitions while leaving dimmed state", NULL},
        {"day-temp", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.temp[DAY], 100, "Daily gamma temperature, between 1000 and 10000", NULL},
        {"night-temp", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.temp[NIGHT], 100, "Nightly gamma temperature, between 1000 and 10000", NULL},
        {"lat", 0, POPT_ARG_DOUBLE, &conf.loc.lat, 100, "Your desired latitude", NULL},
        {"lon", 0, POPT_ARG_DOUBLE, &conf.loc.lon, 100, "Your desired longitude", NULL},
        {"sunrise", 0, POPT_ARG_STRING, NULL, 3, "Force sunrise time for gamma correction", "07:00"},
        {"sunset", 0, POPT_ARG_STRING, NULL, 4, "Force sunset time for gamma correction", "19:00"},
        {"no-gamma", 0, POPT_ARG_NONE, &conf.no_gamma, 100, "Disable gamma correction tool", NULL},
        {"no-dimmer", 0, POPT_ARG_NONE, &conf.no_dimmer, 100, "Disable dimmer tool", NULL},
        {"no-dpms", 0, POPT_ARG_NONE, &conf.no_dpms, 100, "Disable dpms tool", NULL},
        {"no-inhibit", 0, POPT_ARG_NONE, &conf.no_inhibit, 100, "Disable org.freedesktop.PowerManagement.Inhibit support", NULL},
        {"no-backlight", 0, POPT_ARG_NONE, &conf.no_backlight, 100, "Disable backlight module", NULL},
        {"no-screen", 0, POPT_ARG_NONE, &conf.no_screen, 100, "Disable screen module", NULL},
        {"dimmer-pct", 0, POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &conf.dimmer_pct, 100, "Backlight level used while screen is dimmed, in pergentage", NULL},
        {"verbose", 0, POPT_ARG_NONE, &conf.verbose, 100, "Enable verbose mode", NULL},
        {"no-auto-calib", 0, POPT_ARG_NONE, &conf.no_auto_calib, 100, "Disable screen backlight automatic calibration", NULL},
        {"no-kbd-backlight", 0, POPT_ARG_NONE, &conf.no_keyboard_bl, 100, "Disable keyboard backlight calibration", NULL},
        {"shutter-thres", 0, POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &conf.shutter_threshold, 100, "Threshold to consider a capture as clogged", NULL},
        {"version", 'v', POPT_ARG_NONE, NULL, 5, "Show version info", NULL},
        {"conf-file", 'c', POPT_ARG_STRING, NULL, 6, "Specify a conf file to be parsed", NULL},
        {"gamma-long-transition", 0, POPT_ARG_NONE, &conf.gamma_long_transition, 100, "Enable a very long smooth transition for gamma (redshift-like)", NULL },
        {"ambient-gamma", 0, POPT_ARG_NONE, &conf.ambient_gamma, 100, "Enable screen temperature matching ambient brightness instead of time based.", NULL },
        POPT_AUTOHELP
        POPT_TABLEEND
    };

    pc = poptGetContext(NULL, argc, (const char **)argv, po, 0);
    int rc;
    while ((rc = poptGetNextOpt(pc)) > 0) {
        char *str = poptGetOptArg(pc);
        switch (rc) {
            case 1:
                strncpy(conf.dev_name, str, sizeof(conf.dev_name) - 1);
                break;
            case 2:
                strncpy(conf.screen_path, str, sizeof(conf.screen_path) - 1);
                break;
            case 3:
                strncpy(conf.events[SUNRISE], str, sizeof(conf.events[SUNRISE]) - 1);
                break;
            case 4:
                strncpy(conf.events[SUNSET], str, sizeof(conf.events[SUNSET]) - 1);
                break;
            case 5:
                printf("%s: C daemon utility to automagically adjust screen backlight to match ambient brightness.\n"
                        "* Current version: %s\n"
                        "* https://github.com/FedeDP/Clight\n"
                        "* Copyright (C) 2019  Federico Di Pierro <nierro92@gmail.com>\n", argv[0], VERSION);
                exit(EXIT_SUCCESS);
            case 6:
                strncpy(conf_file, str, size);
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

/*
 * It does all needed checks to correctly reset default values
 * in case of wrong options set.
 */
static void check_conf(void) {
    if (conf.timeout[ON_AC][DAY] <= 0) {
        WARN("Wrong day timeout on AC value. Resetting default value.\n");
        conf.timeout[ON_AC][DAY] = 10 * 60;
    }
    if (conf.timeout[ON_AC][NIGHT] <= 0) {
        WARN("Wrong night timeout on AC value. Resetting default value.\n");
        conf.timeout[ON_AC][NIGHT] = 45 * 60;
    }
    if (conf.timeout[ON_AC][IN_EVENT] <= 0) {
        WARN("Wrong event timeout on AC value. Resetting default value.\n");
        conf.timeout[ON_AC][IN_EVENT] = 5 * 60;
    }
    if (conf.timeout[ON_BATTERY][DAY] <= 0) {
        WARN("Wrong day timeout on BATT value. Resetting default value.\n");
        conf.timeout[ON_BATTERY][DAY] = 20 * 60;
    }
    if (conf.timeout[ON_BATTERY][NIGHT] <= 0) {
        WARN("Wrong night timeout on BATT value. Resetting default value.\n");
        conf.timeout[ON_BATTERY][NIGHT] = 90 * 60;
    }
    if (conf.timeout[ON_BATTERY][IN_EVENT] <= 0) {
        WARN("Wrong event timeout on BATT value. Resetting default value.\n");
        conf.timeout[ON_BATTERY][IN_EVENT] = 10 * 60;
    }
    if (conf.num_captures < 1 || conf.num_captures > 20) {
        WARN("Wrong frames value. Resetting default value.\n");
        conf.num_captures = 5;
    }
    if (conf.temp[DAY] < 1000 || conf.temp[DAY] > 10000) {
        WARN("Wrong daily temp value. Resetting default value.\n");
        conf.temp[DAY] = 6500;
    }
    if (conf.temp[NIGHT] < 1000 || conf.temp[NIGHT] > 10000) {
        WARN("Wrong nightly temp value. Resetting default value.\n");
        conf.temp[NIGHT] = 4000;
    }
    if (conf.event_duration <= 0) {
        WARN("Wrong event duration value. Resetting default value.\n");
        conf.event_duration = 30 * 60;
    }
    if (conf.dimmer_pct > 1 || conf.dimmer_pct < 0) {
        WARN("Wrong dimmer backlight percentage value. Resetting default value.\n");
        conf.dimmer_pct = 0.2;
    }
    if (fabs(conf.loc.lat) > 90.0f && conf.loc.lat != LAT_UNDEFINED) {
        WARN("Wrong latitude value. Resetting default value.\n");
        conf.loc.lat = LAT_UNDEFINED;
    }
    if (fabs(conf.loc.lon) > 180.0f && conf.loc.lon != LON_UNDEFINED) {
        WARN("Wrong longitude value. Resetting default value.\n");
        conf.loc.lon = LON_UNDEFINED;
    }
    if (conf.backlight_trans_step <= 0.0f || conf.backlight_trans_step >= 1.0f) {
        WARN("Wrong backlight_trans_step value. Resetting default value.\n");
        conf.backlight_trans_step = 0.05;
    }
    if (conf.dimmer_trans_step[ENTER] <= 0.0f || conf.dimmer_trans_step[ENTER] >= 1.0f) {
        WARN("Wrong dimmer_trans_step[ENTER] value. Resetting default value.\n");
        conf.dimmer_trans_step[ENTER] = 0.05;
    }
    if (conf.dimmer_trans_step[EXIT] <= 0.0f || conf.dimmer_trans_step[EXIT] >= 1.0f) {
        WARN("Wrong dimmer_trans_step[EXIT] value. Resetting default value.\n");
        conf.dimmer_trans_step[EXIT] = 0.05;
    }
    if (conf.gamma_trans_step <= 0) {
        WARN("Wrong gamma_trans_step value. Resetting default value.\n");
        conf.gamma_trans_step = 50;
    }
    if (conf.backlight_trans_timeout <= 0) {
        WARN("Wrong backlight_trans_timeout value. Resetting default value.\n");
        conf.backlight_trans_timeout = 30;
    }
    if (conf.dimmer_trans_timeout[ENTER] <= 0) {
        WARN("Wrong dimmer_trans_timeout[ENTER] value. Resetting default value.\n");
        conf.dimmer_trans_timeout[ENTER] = 30;
    }
    if (conf.dimmer_trans_timeout[EXIT] <= 0) {
        WARN("Wrong dimmer_trans_timeout[EXIT] value. Resetting default value.\n");
        conf.dimmer_trans_timeout[EXIT] = 30;
    }
    if (conf.gamma_trans_timeout <= 0) {
        WARN("Wrong gamma_trans_timeout value. Resetting default value.\n");
        conf.gamma_trans_timeout = 300;
    }
    if (conf.shutter_threshold < 0 || conf.shutter_threshold >= 1) {
        WARN("Wrong shutter_threshold value. Resetting default value.\n");
        conf.shutter_threshold = 0.0;
    }
    
    /* Forcefully enable BACKLIGHT if ambient_gamma is enabled */
    if (conf.ambient_gamma) {
        conf.no_backlight = 0;
    }

    int i, reg_points_ac_needed = 0, reg_points_batt_needed = 0;
    /* Check regression points values */
    for (i = 0; i < conf.num_points[ON_AC] && !reg_points_ac_needed; i++) {
        if (conf.regression_points[ON_AC][i] < 0.0 || conf.regression_points[ON_AC][i] > 1.0) {
            reg_points_ac_needed = 1;
        }
    }
    for (i = 0; i < conf.num_points[ON_BATTERY] && !reg_points_batt_needed; i++) {
        if (conf.regression_points[ON_BATTERY][i] < 0.0 || conf.regression_points[ON_BATTERY][i] > 1.0) {
            reg_points_batt_needed = 1;
        }
    }
    
    if (reg_points_ac_needed) {
        WARN("Wrong ac_regression points. Resetting default values.\n");
        conf.num_points[ON_AC] = DEF_SIZE_POINTS;
        memcpy(conf.regression_points[ON_AC],
               (double[]){ 0.0, 0.15, 0.29, 0.45, 0.61, 0.74, 0.81, 0.88, 0.93, 0.97, 1.0 },
               DEF_SIZE_POINTS * sizeof(double));
    }
    if (reg_points_batt_needed) {
        WARN("Wrong batt_regression points. Resetting default values.\n");
        conf.num_points[ON_BATTERY] = DEF_SIZE_POINTS;
        memcpy(conf.regression_points[ON_BATTERY],
               (double[]){ 0.0, 0.15, 0.23, 0.36, 0.52, 0.59, 0.65, 0.71, 0.75, 0.78, 0.80 },
               DEF_SIZE_POINTS * sizeof(double));
    }

    for (i = 0; i < SIZE_EVENTS; i++) {
        struct tm timeinfo;
        if (strlen(conf.events[i]) && !strptime(conf.events[i], "%R", &timeinfo)) {
            memset(conf.events[i], 0, sizeof(conf.events[i]));
        }
    }
    
    if (conf.screen_contrib < 0 || conf.screen_contrib >= 1) {
        WARN("Wrong screen_contrib value. Resetting default value.\n");
        conf.screen_contrib = 0.1;
    }
    
    if (conf.screen_samples < 0) {
        WARN("Wrong screen_samples value. Resetting default value.\n");
        conf.screen_samples = 10;
    }
}
