#include "../inc/config.h"
#include "../inc/opts.h"
#include <popt.h>

static void parse_cmd(int argc, char *const argv[]);
static void check_conf(void);

/*
 * Init default config values,
 * parse both global and user-local config files through libconfig,
 * and parse cmdline args through popt lib.
 * Finally, check configuration values and log it.
 */
void init_opts(int argc, char *argv[]) {
    /* default values */
    conf.num_captures = 5;
    conf.timeout[ON_AC][DAY] = 10 * 60;
    conf.timeout[ON_AC][NIGHT] = 45 * 60;
    conf.timeout[ON_AC][EVENT] = 5 * 60;
    conf.timeout[ON_BATTERY][DAY] = 2 * conf.timeout[ON_AC][DAY];
    conf.timeout[ON_BATTERY][NIGHT] = 2 * conf.timeout[ON_AC][NIGHT];
    conf.timeout[ON_BATTERY][EVENT] = 2 * conf.timeout[ON_AC][EVENT];
    conf.timeout[ON_AC][UNKNOWN] = conf.timeout[ON_AC][DAY]; // if unknown, fallback to 10mins
    conf.timeout[ON_BATTERY][UNKNOWN] = conf.timeout[ON_BATTERY][DAY]; // if unknown, fallback to 10mins
    conf.temp[DAY] = 6500;
    conf.temp[NIGHT] = 4000;
    conf.temp[EVENT] = -1;
    conf.temp[UNKNOWN] = conf.temp[DAY];
    conf.event_duration = 30 * 60;
    conf.dimmer_timeout[ON_AC] = 300;
    conf.dimmer_timeout[ON_BATTERY] = 45;
    conf.dimmer_pct = 20;
    
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
    memcpy(conf.regression_points[ON_AC], 
           (double[]){ 0.0, 0.15, 0.29, 0.45, 0.61, 0.74, 0.81, 0.88, 0.93, 0.97, 1.0 }, 
           SIZE_POINTS * sizeof(double));
    memcpy(conf.regression_points[ON_BATTERY], 
           (double[]){ 0.0, 0.15, 0.23, 0.36, 0.52, 0.59, 0.65, 0.71, 0.75, 0.78, 0.80 }, 
           SIZE_POINTS * sizeof(double));
    
    /* Default dpms timeouts ON AC */
    memcpy(conf.dpms_timeouts[ON_AC], 
           (int[]){ 900, 1200, 1800 }, 
           SIZE_DPMS * sizeof(int));
    
    /* Default dpms timeouts ON BATT */
    memcpy(conf.dpms_timeouts[ON_BATTERY], 
           (int[]){ 300, 420, 600 }, 
           SIZE_DPMS * sizeof(int));

    read_config(GLOBAL);
    read_config(LOCAL);
    parse_cmd(argc, argv);
    check_conf();
}

/*
 * Parse cmdline to get cmd line options
 */
static void parse_cmd(int argc, char *const argv[]) {
    poptContext pc;
    const struct poptOption po[] = {
        {"capture", 'c', POPT_ARG_NONE, &conf.single_capture_mode, 100, "Take a fast capture/screen brightness calibration and quit", NULL},
        {"frames", 'f', POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.num_captures, 100, "Frames taken for each capture, Between 1 and 20", NULL},
        {"ac-day-timeout", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.timeout[ON_AC][DAY], 100, "Seconds between each capture during the day on AC", NULL},
        {"ac-night-timeout", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.timeout[ON_AC][NIGHT], 100, "Seconds between each capture during the night on AC", NULL},
        {"ac-event-timeout", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.timeout[ON_AC][EVENT], 100, "Seconds between each capture during an event(sunrise, sunset) on AC", NULL},
        {"batt-day-timeout", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.timeout[ON_BATTERY][DAY], 100, "Seconds between each capture during the day on battery", NULL},
        {"batt-night-timeout", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.timeout[ON_BATTERY][NIGHT], 100, "Seconds between each capture during the night on battery", NULL},
        {"batt-event-timeout", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.timeout[ON_BATTERY][EVENT], 100, "Seconds between each capture during an event(sunrise, sunset) on battery", NULL},
        {"device", 'd', POPT_ARG_STRING, NULL, 1, "Path to webcam device. By default, first matching device is used", "video0"},
        {"backlight", 'b', POPT_ARG_STRING, NULL, 2, "Path to backlight syspath. By default, first matching device is used", "intel_backlight"},
        {"no-gamma-smooth", 0, POPT_ARG_NONE, &modules[GAMMA_SMOOTH].state, 100, "Disable smooth gamma transition", NULL},
        {"no-dimmer-smooth", 0, POPT_ARG_NONE, &modules[DIMMER_SMOOTH].state, 100, "Disable smooth dimmer transition", NULL},
        {"day-temp", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.temp[DAY], 100, "Daily gamma temperature, between 1000 and 10000", NULL},
        {"night-temp", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.temp[NIGHT], 100, "Nightly gamma temperature, between 1000 and 10000", NULL},
        {"lat", 0, POPT_ARG_DOUBLE, &conf.lat, 100, "Your desired latitude", NULL},
        {"lon", 0, POPT_ARG_DOUBLE, &conf.lon, 100, "Your desired longitude", NULL},
        {"sunrise", 0, POPT_ARG_STRING, NULL, 3, "Force sunrise time for gamma correction", "07:00"},
        {"sunset", 0, POPT_ARG_STRING, NULL, 4, "Force sunset time for gamma correction", "19:00"},
        {"no-gamma", 0, POPT_ARG_NONE, &modules[GAMMA].state, 100, "Disable gamma correction tool", NULL},
        {"event-duration", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.event_duration, 100, "Duration of an event in seconds: an event starts event_duration seconds before real sunrise/sunset time and ends event_duration seconds after", NULL},
        {"dimmer-pct", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.dimmer_pct, 100, "Backlight level used while screen is dimmed, in pergentage", NULL},
        {"no-dimmer", 0, POPT_ARG_NONE, &modules[DIMMER].state, 100, "Disable dimmer tool", NULL},
        {"ac-dimmer-timeout", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.dimmer_timeout[ON_AC], 100, "Seconds of inactivity before dimmin screen on AC", NULL},
        {"batt-dimmer-timeout", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.dimmer_timeout[ON_BATTERY], 100, "Seconds of inactivity before dimmin screen on battery", NULL},
        {"no-dpms", 0, POPT_ARG_NONE, &modules[DPMS].state, 100, "Disable dpms tool", NULL},
        {"verbose", 0, POPT_ARG_NONE, &conf.verbose, 100, "Enable verbose mode", NULL},
        {"version", 'v', POPT_ARG_NONE, NULL, 5, "Show version info", NULL},
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
                printf("%s version: %s\n", argv[0], VERSION);
                exit(EXIT_SUCCESS);
            default:
                break;
        }
        if (str) {
            free(str);
        }
    }
    // poptGetNextOpt returns -1 when the final argument has been parsed
    // otherwise an error occured
    if (rc != -1) {
        ERROR("%s\n", poptStrerror(rc));
    }
    poptFreeContext(pc);
}

/* 
 * It does all needed checks to correctly reset default values
 * in case of wrong options setted.
 */
static void check_conf(void) {
    /*
     * Reset default values in case of wrong values
     */
    if (conf.timeout[ON_AC][DAY] <= 0) {
        WARN("Wrong day timeout on AC value. Resetting default value.\n");
        conf.timeout[ON_AC][DAY] = 10 * 60;
    }
    if (conf.timeout[ON_AC][NIGHT] <= 0) {
        WARN("Wrong night timeout on AC value. Resetting default value.\n");
        conf.timeout[ON_AC][NIGHT] = 45 * 60;
    }
    if (conf.timeout[ON_AC][EVENT] <= 0) {
        WARN("Wrong event timeout on AC value. Resetting default value.\n");
        conf.timeout[ON_AC][EVENT] = 5 * 60;
    }
    if (conf.timeout[ON_BATTERY][DAY] <= 0) {
        WARN("Wrong day timeout on BATT value. Resetting default value.\n");
        conf.timeout[ON_BATTERY][DAY] = 20 * 60;
    }
    if (conf.timeout[ON_BATTERY][NIGHT] <= 0) {
        WARN("Wrong night timeout on BATT value. Resetting default value.\n");
        conf.timeout[ON_BATTERY][NIGHT] = 90 * 60;
    }
    if (conf.timeout[ON_BATTERY][EVENT] <= 0) {
        WARN("Wrong event timeout on BATT value. Resetting default value.\n");
        conf.timeout[ON_BATTERY][EVENT] = 10 * 60;
    }
    if (conf.num_captures <= 0 || conf.num_captures > 20) {
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
    if (conf.dimmer_pct > 100 || conf.dimmer_pct < 0) {
        WARN("Wrong dimmer backlight percentage value. Resetting default value.\n");
        conf.dimmer_pct = 20;
    }
    if (conf.dimmer_timeout[ON_AC] <= 0) {
        WARN("Wrong dimmer timeout on AC value. Resetting default value.\n");
        conf.dimmer_timeout[ON_AC] = 300;
    }
    if (conf.dimmer_timeout[ON_BATTERY] <= 0) {
        WARN("Wrong dimmer timeout on BATT value. Resetting default value.\n");
        conf.dimmer_timeout[ON_BATTERY] = 45;
    }
    
    int i, reg_points_ac_needed = 0, reg_points_batt_needed = 0;
    /* Check regression points values */
    for (i = 0; i < SIZE_POINTS && !reg_points_batt_needed && !reg_points_ac_needed; i++) {
        if (!reg_points_ac_needed && (conf.regression_points[ON_AC][i] < 0.0 || conf.regression_points[ON_AC][i] > 1.0)) {
            reg_points_ac_needed = 1;
        }
        
        if (!reg_points_batt_needed && (conf.regression_points[ON_AC][i] < 0.0 || conf.regression_points[ON_AC][i] > 1.0)) {
            reg_points_batt_needed = 1;
        }
        
    }
    if (reg_points_ac_needed) {
        WARN("Wrong ac_regression points. Resetting default values.\n");
        memcpy(conf.regression_points[ON_AC], 
               (double[]){ 0.0, 0.15, 0.29, 0.45, 0.61, 0.74, 0.81, 0.88, 0.93, 0.97, 1.0 }, 
               SIZE_POINTS * sizeof(double));
    }
    if (reg_points_batt_needed) {
        WARN("Wrong batt_regression points. Resetting default values.\n");
        memcpy(conf.regression_points[ON_BATTERY], 
               (double[]){ 0.0, 0.15, 0.23, 0.36, 0.52, 0.59, 0.65, 0.71, 0.75, 0.78, 0.80 }, 
               SIZE_POINTS * sizeof(double));
    }
    
    /* Check dpms timeout values */
    int dpms_timeout_ac_needed = 0, dpms_timeout_batt_needed = 0;
    for (i = 0; i < SIZE_DPMS && !dpms_timeout_ac_needed && !dpms_timeout_batt_needed; i++) {
        if (conf.dpms_timeouts[ON_AC][i] <= 0) {
            dpms_timeout_ac_needed = 1;
        }
        if (conf.dpms_timeouts[ON_BATTERY][i] <= 0) {
            dpms_timeout_batt_needed = 1;
        }
        
    }
    if (dpms_timeout_ac_needed) {
        WARN("Wrong on ac dpms timeouts. Resetting default values.\n");
        memcpy(conf.dpms_timeouts[ON_AC], 
               (int[]){ 900, 1200, 1800 }, 
               SIZE_DPMS * sizeof(int));
    }
    if (dpms_timeout_batt_needed) {
        WARN("Wrong on batt dpms timeouts. Resetting default values.\n");
        memcpy(conf.dpms_timeouts[ON_BATTERY], 
               (int[]){ 300, 420, 600 }, 
               SIZE_DPMS * sizeof(int));
    }
    
}
