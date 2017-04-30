#include "../inc/opts.h"
#include <popt.h>

static void parse_cmd(int argc, char *const argv[]);

/*
 * Init default config values,
 * parse both global and user-local config files through libconfig,
 * and parse cmdline args through popt lib.
 * Finally, check configuration values and log it.
 */
void init_opts(int argc, char *argv[]) {
    /* default values */
    conf.num_captures = 5;
    conf.timeout[DAY] = 10 * 60; // 10 mins during the day
    conf.timeout[NIGHT] = 45 * 60; // 45 mins during the night
    conf.timeout[EVENT] = 3 * 60; // 3 mins during an event
    conf.timeout[UNKNOWN] = conf.timeout[DAY]; // if unknown, fallback to 10mins
    conf.temp[DAY] = 6500;
    conf.temp[NIGHT] = 4000;
    conf.temp[EVENT] = -1;
    conf.temp[UNKNOWN] = conf.temp[DAY];
    conf.event_duration = 30 * 60;

    read_config(GLOBAL);
    read_config(LOCAL);
    parse_cmd(argc, argv);
}

/**
 * Parse cmdline to get cmd line options
 */
static void parse_cmd(int argc, char *const argv[]) {
    poptContext pc;
    struct poptOption po[] = {
        {"capture", 'c', POPT_ARG_NONE, &conf.single_capture_mode, 0, "Take a fast capture/screen brightness calibration and quit", NULL},
        {"frames", 'f', POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.num_captures, 0, "Frames taken for each capture, Between 1 and 20.", NULL},
        {"day_timeout", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.timeout[DAY], 0, "Seconds between each capture during the day.", NULL},
        {"night_timeout", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.timeout[NIGHT], 0, "Seconds between each capture during the night.", NULL},
        {"event_timeout", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.timeout[EVENT], 0, "Seconds between each capture during an event(sunrise, sunset).", NULL},
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
        {"lowest_backlight", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.lowest_backlight_level, 0, "Lowest backlight level that clight can set.", NULL},
        {"event_duration", 0, POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &conf.event_duration, 0, "Duration of an event in seconds: an event starts event_duration seconds before real sunrise/sunset time and ends event_duration seconds after.", NULL},
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
 * Moreover, it does required check to disable various modules
 * in case eg: --no-gamma has been passed.
 */
void check_conf(void) {
    /*
     * Reset default values in case of wrong values
     */
    if (conf.timeout[DAY] <= 0) {
        WARN("Wrong day timeout value. Resetting default value.\n");
        conf.timeout[DAY] = 10 * 60;
    }
    if (conf.timeout[NIGHT] <= 0) {
        WARN("Wrong night timeout value. Resetting default value.\n");
        conf.timeout[NIGHT] = 45 * 60;
    }
    if (conf.timeout[EVENT] <= 0) {
        WARN("Wrong event timeout value. Resetting default value.\n");
        conf.timeout[EVENT] = 3 * 60;
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
    
    /* Disable gamma if in single capture mode, or if --no-gamma option was setted */
    if (conf.single_capture_mode || conf.no_gamma) {
        disable_module(GAMMA_IX);
    } else if ((strlen(conf.events[SUNRISE]) && strlen(conf.events[SUNSET])) || (conf.lat != 0.0 && conf.lon != 0.0)) {
        /* If sunrise and sunset times, or lat and lon, are both passed, disable LOCATION (but not gamma, by setting a SOFT dep instead of HARD) */
        modules[GAMMA_IX].self->deps[1].type = SOFT;
        disable_module(LOCATION_IX);
    }
    
    /* Disable gamma support if we're not in a X session */
    if (!modules[GAMMA_IX].disabled && (!getenv("XDG_SESSION_TYPE") || strcmp(getenv("XDG_SESSION_TYPE"), "x11"))) {
        WARN("Disabling gamma support as X is not running.\n");
        disable_module(GAMMA_IX);
    }
}
