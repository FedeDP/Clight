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
    conf.timeout[ON_AC][EVENT] = 3 * 60;
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
    conf.max_backlight_pct[ON_AC] = 100;
    conf.max_backlight_pct[ON_BATTERY] = 100;

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
    struct poptOption po[] = {
        {"capture", 'c', POPT_ARG_NONE, &conf.single_capture_mode, 0, "Take a fast capture/screen brightness calibration and quit", NULL},
        {"frames", 'f', POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.num_captures, 0, "Frames taken for each capture, Between 1 and 20", NULL},
        {"ac_day_timeout", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.timeout[ON_AC][DAY], 0, "Seconds between each capture during the day on AC", NULL},
        {"ac_night_timeout", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.timeout[ON_AC][NIGHT], 0, "Seconds between each capture during the night on AC", NULL},
        {"ac_event_timeout", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.timeout[ON_AC][EVENT], 0, "Seconds between each capture during an event(sunrise, sunset) on AC", NULL},
        {"batt_day_timeout", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.timeout[ON_BATTERY][DAY], 0, "Seconds between each capture during the day on battery", NULL},
        {"batt_night_timeout", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.timeout[ON_BATTERY][NIGHT], 0, "Seconds between each capture during the night on battery", NULL},
        {"batt_event_timeout", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.timeout[ON_BATTERY][EVENT], 0, "Seconds between each capture during an event(sunrise, sunset) on battery", NULL},
        {"device", 'd', POPT_ARG_STRING, NULL, 1, "Path to webcam device. By default, first matching device is used", "video0"},
        {"backlight", 'b', POPT_ARG_STRING, NULL, 2, "Path to backlight syspath. By default, first matching device is used", "intel_backlight"},
        {"no-smooth_transition", 0, POPT_ARG_NONE, &conf.no_smooth_transition, 0, "Disable smooth gamma transition", NULL},
        {"day_temp", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.temp[DAY], 0, "Daily gamma temperature, between 1000 and 10000", NULL},
        {"night_temp", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.temp[NIGHT], 0, "Nightly gamma temperature, between 1000 and 10000", NULL},
        {"lat", 0, POPT_ARG_DOUBLE, &conf.lat, 0, "Your desired latitude", NULL},
        {"lon", 0, POPT_ARG_DOUBLE, &conf.lon, 0, "Your desired longitude", NULL},
        {"sunrise", 0, POPT_ARG_STRING, NULL, 3, "Force sunrise time for gamma correction", "07:00"},
        {"sunset", 0, POPT_ARG_STRING, NULL, 4, "Force sunset time for gamma correction", "19:00"},
        {"no-gamma", 0, POPT_ARG_NONE, &conf.no_gamma, 0, "Disable gamma correction tool", NULL},
        {"lowest_backlight", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.lowest_backlight_level, 0, "Lowest backlight level that clight can set", NULL},
        {"batt_max_backlight_pct", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.max_backlight_pct[ON_BATTERY], 0, "Max backlight level that clight can set while on battery, in percentage", NULL},
        {"event_duration", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.event_duration, 0, "Duration of an event in seconds: an event starts event_duration seconds before real sunrise/sunset time and ends event_duration seconds after", NULL},
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
        }
        free(str);
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
        WARN("Wrong day timeout value. Resetting default value.\n");
        conf.timeout[ON_AC][DAY] = 10 * 60;
    }
    if (conf.timeout[ON_AC][NIGHT] <= 0) {
        WARN("Wrong night timeout value. Resetting default value.\n");
        conf.timeout[ON_AC][NIGHT] = 45 * 60;
    }
    if (conf.timeout[ON_AC][EVENT] <= 0) {
        WARN("Wrong event timeout value. Resetting default value.\n");
        conf.timeout[ON_AC][EVENT] = 3 * 60;
    }
    if (conf.timeout[ON_BATTERY][DAY] <= 0) {
        WARN("Wrong day timeout value. Resetting default value.\n");
        conf.timeout[ON_BATTERY][DAY] = 10 * 60;
    }
    if (conf.timeout[ON_BATTERY][NIGHT] <= 0) {
        WARN("Wrong night timeout value. Resetting default value.\n");
        conf.timeout[ON_BATTERY][NIGHT] = 45 * 60;
    }
    if (conf.timeout[ON_BATTERY][EVENT] <= 0) {
        WARN("Wrong event timeout value. Resetting default value.\n");
        conf.timeout[ON_BATTERY][EVENT] = 3 * 60;
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
    if (conf.max_backlight_pct[ON_BATTERY] > 100 || conf.max_backlight_pct[ON_BATTERY] < 0) {
        WARN("Wrong on battery max backlight percentage value. Resetting default value.\n");
        conf.max_backlight_pct[ON_BATTERY] = 100;
    }
}
