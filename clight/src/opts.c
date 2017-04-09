#include "../inc/opts.h"
#include <popt.h>

static void parse_cmd(int argc, char *const argv[]);

/*
 * Init default config values and parse cmdline args through popt lib.
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
    conf.temp[UNKNOWN] = 6500;
    conf.smooth_transition = 1;

    parse_cmd(argc, argv);

    /*
     * Reset default values in case of wrong values
     */
    if (conf.timeout[DAY] <= 0) {
        ERROR("Wrong day timeout value. Resetting default value.\n");
        conf.timeout[DAY] = 10 * 60;
    }

    if (conf.timeout[NIGHT] <= 0) {
        ERROR("Wrong night timeout value. Resetting default value.\n");
        conf.timeout[NIGHT] = 45 * 60;
    }

    if (conf.num_captures <= 0) {
        ERROR("Wrong frames value. Resetting default value.\n");
        conf.num_captures = 5;
    }

    if (conf.temp[DAY] < 1000 || conf.temp[DAY] > 10000) {
        ERROR("Wrong daily temp value. Resetting default value.\n");
        conf.temp[DAY] = 6500;
    }

    if (conf.temp[NIGHT] < 1000 || conf.temp[NIGHT] > 10000) {
        ERROR("Wrong nightly temp value. Resetting default value.\n");
        conf.temp[NIGHT] = 4000;
    }

    log_conf();
}

/**
 * Parse cmdline to get cmd line options
 */
static void parse_cmd(int argc, char *const argv[]) {
    poptContext pc;
    struct poptOption po[] = {
        {"capture", 'c', POPT_ARG_NONE, &conf.single_capture_mode, 0, "Take a fast capture/screen brightness calibration and quit.", NULL},
        {"frames", 'f', POPT_ARG_INT, &conf.num_captures, 0, "Frames taken for each capture. Defaults to 5.", "Number of frames to be taken."},
        {"day_timeout", 0, POPT_ARG_INT, &conf.timeout[DAY], 0, "Timeout between captures during the day. Defaults to 10mins.", "Number of seconds between each capture."},
        {"night_timeout", 0, POPT_ARG_INT, &conf.timeout[NIGHT], 0, "Timeout between captures during the night. Defaults to 45mins.", "Number of seconds between each capture."},
        {"device", 'd', POPT_ARG_STRING, &conf.dev_name, 0, "Path to webcam device. By default, first matching device is used.", "Webcam device to be used."},
        {"backlight", 'b', POPT_ARG_STRING, &conf.screen_path, 0, "Path to backlight syspath. By default, first matching device is used.", "Backlight to be used."},
        {"smooth_transition", 0, POPT_ARG_INT, &conf.smooth_transition, 0, "Whether to enable smooth gamma transition.", "1 enable/0 disable."},
        {"day_temp", 0, POPT_ARG_INT, &conf.temp[DAY], 0, "Daily gamma temperature.", "Between 1000 and 10000."},
        {"night_temp", 0, POPT_ARG_INT, &conf.temp[NIGHT], 0, "Nightly gamma temperature.", "Between 1000 and 10000."},
        {"lat", 0, POPT_ARG_DOUBLE, &conf.lat, 0, "Your current latitude.", NULL},
        {"lon", 0, POPT_ARG_DOUBLE, &conf.lon, 0, "Your current longitude.", NULL},
        {"sunrise", 0, POPT_ARG_STRING, &conf.events[SUNRISE], 0, "Force a sunrise time when switch gamma temp.", "Sunrise time, eg: 07:00."},
        {"sunset", 0, POPT_ARG_STRING, &conf.events[SUNSET], 0, "Force a sunset time when switch gamma temp.", "Sunset time, eg: 19:00."},
        POPT_AUTOHELP
        {NULL}
    };

    pc = poptGetContext(NULL, argc, (const char **)argv, po, 0);
    // process options and handle each val returned
    int rc = poptGetNextOpt(pc);
    // poptGetNextOpt returns -1 when the final argument has been parsed
    // otherwise an error occured
    if (rc != -1) {
        ERROR("%s\n", poptStrerror(rc));
        poptFreeContext(pc);
        exit(1);
    }
    poptFreeContext(pc);
}

void destroy_opts(void) {
    if (conf.events[SUNRISE]) {
        free(conf.events[SUNRISE]);
    }
    if (conf.events[SUNSET]) {
        free(conf.events[SUNSET]);
    }
    if (conf.dev_name) {
        free(conf.dev_name);
    }
    if (conf.screen_path) {
        free(conf.screen_path);
    }
}
