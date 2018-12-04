#include <config.h>
#include <libconfig.h>

static void init_config_file(enum CONFIG file, char *filename);

/*
 * Use getpwuid to get user home dir
 */
static void init_config_file(enum CONFIG file, char *filename) {
    if (file == LOCAL) {
        if (getenv("XDG_CONFIG_HOME")) {
            snprintf(filename, PATH_MAX, "%s/clight.conf", getenv("XDG_CONFIG_HOME"));
        } else {
            snprintf(filename, PATH_MAX, "%s/.config/clight.conf", getpwuid(getuid())->pw_dir);
        }
    } else {
        snprintf(filename, PATH_MAX, "%s/clight.conf", CONFDIR);
    }
}

int read_config(enum CONFIG file) {
    int r = 0;
    char config_file[PATH_MAX + 1] = {0};
    config_t cfg;
    const char *sensor_dev, *screendev, *sunrise, *sunset;

    init_config_file(file, config_file);
    if (access(config_file, F_OK) == -1) {
        WARN("Config file %s not found.\n", config_file);
        return -1;
    }

    config_init(&cfg);
    if (config_read_file(&cfg, config_file) == CONFIG_TRUE) {
        config_lookup_int(&cfg, "captures", &conf.num_captures);
        config_lookup_bool(&cfg, "no_smooth_backlight_transition", &conf.no_smooth_backlight);
        config_lookup_bool(&cfg, "no_smooth_gamma_transition", &conf.no_smooth_gamma);
        config_lookup_bool(&cfg, "no_smooth_dimmer_transition", &conf.no_smooth_dimmer);
        config_lookup_float(&cfg, "backlight_trans_step", &conf.backlight_trans_step);
        config_lookup_int(&cfg, "gamma_trans_step", &conf.gamma_trans_step);
        config_lookup_float(&cfg, "dimmer_trans_step", &conf.dimmer_trans_step);
        config_lookup_int(&cfg, "backlight_trans_timeout", &conf.backlight_trans_timeout);
        config_lookup_int(&cfg, "gamma_trans_timeout", &conf.gamma_trans_timeout);
        config_lookup_int(&cfg, "dimmer_trans_timeout", &conf.dimmer_trans_timeout);
        config_lookup_bool(&cfg, "no_backlight", (int *)&modules[BACKLIGHT].state);
        config_lookup_bool(&cfg, "no_gamma", (int *)&modules[GAMMA].state);
        config_lookup_float(&cfg, "latitude", &conf.loc.lat);
        config_lookup_float(&cfg, "longitude", &conf.loc.lon);
        config_lookup_int(&cfg, "event_duration", &conf.event_duration);
        config_lookup_bool(&cfg, "no_dimmer", (int *)&modules[DIMMER].state);
        config_lookup_float(&cfg, "dimmer_pct", &conf.dimmer_pct);
        config_lookup_float(&cfg, "shutter_threshold", &conf.shutter_threshold);
        config_lookup_bool(&cfg, "no_dpms", (int *)&modules[DPMS].state);
        config_lookup_bool(&cfg, "no_inhibit", (int *)&modules[INHIBIT].state);
        config_lookup_bool(&cfg, "verbose", &conf.verbose);
        config_lookup_bool(&cfg, "no_auto_calibration", &conf.no_auto_calib);
        config_lookup_bool(&cfg, "no_kdb_backlight", &conf.no_keyboard_bl);

        if (config_lookup_string(&cfg, "sensor_devname", &sensor_dev) == CONFIG_TRUE) {
            strncpy(conf.dev_name, sensor_dev, sizeof(conf.dev_name) - 1);
        }
        if (config_lookup_string(&cfg, "screen_sysname", &screendev) == CONFIG_TRUE) {
            strncpy(conf.screen_path, screendev, sizeof(conf.screen_path) - 1);
        }
        if (config_lookup_string(&cfg, "sunrise", &sunrise) == CONFIG_TRUE) {
            strncpy(conf.events[SUNRISE], sunrise, sizeof(conf.events[SUNRISE]) - 1);
        }
        if (config_lookup_string(&cfg, "sunset", &sunset) == CONFIG_TRUE) {
            strncpy(conf.events[SUNSET], sunset, sizeof(conf.events[SUNSET]) - 1);
        }

        config_setting_t *points, *root, *timeouts, *gamma;
        root = config_root_setting(&cfg);

        /* Load regression points for backlight curve */
        if ((points = config_setting_get_member(root, "ac_backlight_regression_points"))) {
            if (config_setting_length(points) == SIZE_POINTS) {
                for (int i = 0; i < SIZE_POINTS; i++) {
                    conf.regression_points[ON_AC][i] = config_setting_get_float_elem(points, i);
                }
            } else {
                WARN("Wrong number of ac_backlight_regression_points array elements.\n");
            }
        }

        /* Load regression points for backlight curve */
        if ((points = config_setting_get_member(root, "batt_backlight_regression_points"))) {
            if (config_setting_length(points) == SIZE_POINTS) {
                for (int i = 0; i < SIZE_POINTS; i++) {
                    conf.regression_points[ON_BATTERY][i] = config_setting_get_float_elem(points, i);
                }
            } else {
                WARN("Wrong number of batt_backlight_regression_points array elements.\n");
            }
        }

        /* Load dpms timeouts while on ac */
        if ((timeouts = config_setting_get_member(root, "ac_dpms_timeouts"))) {
            if (config_setting_length(timeouts) == SIZE_DPMS) {
                for (int i = 0; i < SIZE_DPMS; i++) {
                    conf.dpms_timeouts[ON_AC][i] = config_setting_get_int_elem(timeouts, i);
                }
            } else {
                WARN("Wrong number of ac_dpms_timeouts array elements.\n");
            }
        }

        /* Load dpms timeouts while on battery */
        if ((timeouts = config_setting_get_member(root, "batt_dpms_timeouts"))) {
            if (config_setting_length(timeouts) == SIZE_DPMS) {
                for (int i = 0; i < SIZE_DPMS; i++) {
                    conf.dpms_timeouts[ON_BATTERY][i] = config_setting_get_int_elem(timeouts, i);
                }
            } else {
                WARN("Wrong number of batt_dpms_timeouts array elements.\n");
            }
        }

        /* Load capture timeouts while on battery */
        if ((timeouts = config_setting_get_member(root, "ac_capture_timeouts"))) {
            if (config_setting_length(timeouts) == SIZE_STATES) {
                for (int i = 0; i < SIZE_STATES; i++) {
                    conf.timeout[ON_AC][i] = config_setting_get_int_elem(timeouts, i);
                }
            } else {
                WARN("Wrong number of ac_capture_timeouts array elements.\n");
            }
        }

        /* Load capture timeouts while on battery */
        if ((timeouts = config_setting_get_member(root, "batt_capture_timeouts"))) {
            if (config_setting_length(timeouts) == SIZE_STATES) {
                for (int i = 0; i < SIZE_STATES; i++) {
                    conf.timeout[ON_BATTERY][i] = config_setting_get_int_elem(timeouts, i);
                }
            } else {
                WARN("Wrong number of batt_capture_timeouts array elements.\n");
            }
        }

        /* Load dimmer timeouts */
        if ((timeouts = config_setting_get_member(root, "dimmer_timeouts"))) {
            if (config_setting_length(timeouts) == SIZE_AC) {
                for (int i = 0; i < SIZE_AC; i++) {
                    conf.dimmer_timeout[i] = config_setting_get_int_elem(timeouts, i);
                }
            } else {
                WARN("Wrong number of dimmer_timeouts array elements.\n");
            }
        }

        /* Load gamma temperatures -> SIZE_STATES - 1 because temp[EVENT] is not exposed */
        if ((gamma = config_setting_get_member(root, "gamma_temp"))) {
            if (config_setting_length(gamma) == SIZE_STATES - 1) {
                for (int i = 0; i < SIZE_STATES - 1; i++) {
                    conf.temp[i] = config_setting_get_int_elem(gamma, i);
                }
            } else {
                WARN("Wrong number of gamma_temp array elements.\n");
            }
        }

    } else {
        WARN("Config file: %s at line %d.\n",
                config_error_text(&cfg),
                config_error_line(&cfg));
        r = -1;
    }
    config_destroy(&cfg);
    return r;
}

int store_config(enum CONFIG file) {
    int r = 0;
    config_t cfg;
    char config_file[PATH_MAX + 1] = {0};

    init_config_file(file, config_file);
    if (access(config_file, F_OK) != -1) {
        WARN("Config file %s already present. Overwriting.\n", config_file);
    }
    config_init(&cfg);

    config_setting_t *root = config_root_setting(&cfg);
    config_setting_t *setting = config_setting_add(root, "captures", CONFIG_TYPE_INT);
    config_setting_set_int(setting, conf.num_captures);

    setting = config_setting_add(root, "no_smooth_backlight_transition", CONFIG_TYPE_BOOL);
    config_setting_set_bool(setting, conf.no_smooth_backlight);

    setting = config_setting_add(root, "no_smooth_gamma_transition", CONFIG_TYPE_BOOL);
    config_setting_set_bool(setting, conf.no_smooth_gamma);

    setting = config_setting_add(root, "no_smooth_dimmer_transition", CONFIG_TYPE_BOOL);
    config_setting_set_bool(setting, conf.no_smooth_dimmer);

    setting = config_setting_add(root, "backlight_trans_step", CONFIG_TYPE_FLOAT);
    config_setting_set_float(setting, conf.backlight_trans_step);

    setting = config_setting_add(root, "gamma_trans_step", CONFIG_TYPE_INT);
    config_setting_set_int(setting, conf.gamma_trans_step);

    setting = config_setting_add(root, "dimmer_trans_step", CONFIG_TYPE_FLOAT);
    config_setting_set_float(setting, conf.dimmer_trans_step);

    setting = config_setting_add(root, "backlight_trans_timeout", CONFIG_TYPE_INT);
    config_setting_set_int(setting, conf.backlight_trans_timeout);

    setting = config_setting_add(root, "gamma_trans_timeout", CONFIG_TYPE_INT);
    config_setting_set_int(setting, conf.gamma_trans_timeout);

    setting = config_setting_add(root, "dimmer_trans_timeout", CONFIG_TYPE_INT);
    config_setting_set_int(setting, conf.dimmer_trans_timeout);

    setting = config_setting_add(root, "latitude", CONFIG_TYPE_FLOAT);
    config_setting_set_float(setting, conf.loc.lat);

    setting = config_setting_add(root, "longitude", CONFIG_TYPE_FLOAT);
    config_setting_set_float(setting, conf.loc.lon);

    setting = config_setting_add(root, "event_duration", CONFIG_TYPE_INT);
    config_setting_set_int(setting, conf.event_duration);

    setting = config_setting_add(root, "dimmer_pct", CONFIG_TYPE_FLOAT);
    config_setting_set_float(setting, conf.dimmer_pct);

    setting = config_setting_add(root, "verbose", CONFIG_TYPE_BOOL);
    config_setting_set_bool(setting, conf.verbose);

    setting = config_setting_add(root, "no_auto_calibration", CONFIG_TYPE_BOOL);
    config_setting_set_bool(setting, conf.no_auto_calib);

    setting = config_setting_add(root, "no_kdb_backlight", CONFIG_TYPE_BOOL);
    config_setting_set_bool(setting, conf.no_keyboard_bl);

    setting = config_setting_add(root, "sensor_devname", CONFIG_TYPE_STRING);
    config_setting_set_string(setting, conf.dev_name);

    setting = config_setting_add(root, "screen_sysname", CONFIG_TYPE_STRING);
    config_setting_set_string(setting, conf.screen_path);

    setting = config_setting_add(root, "sunrise", CONFIG_TYPE_STRING);
    config_setting_set_string(setting, conf.events[SUNRISE]);

    setting = config_setting_add(root, "sunset", CONFIG_TYPE_STRING);
    config_setting_set_string(setting, conf.events[SUNSET]);

    setting = config_setting_add(root, "shutter_threshold", CONFIG_TYPE_FLOAT);
    config_setting_set_float(setting, conf.shutter_threshold);

    /* -1 here below means append to end of array */
    setting = config_setting_add(root, "ac_backlight_regression_points", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_POINTS; i++) {
        config_setting_set_float_elem(setting, -1, conf.regression_points[ON_AC][i]);
    }

    setting = config_setting_add(root, "batt_backlight_regression_points", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_POINTS; i++) {
        config_setting_set_float_elem(setting, -1, conf.regression_points[ON_BATTERY][i]);
    }

    setting = config_setting_add(root, "ac_dpms_timeouts", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_DPMS; i++) {
        config_setting_set_int_elem(setting, -1, conf.dpms_timeouts[ON_AC][i]);
    }

    setting = config_setting_add(root, "batt_dpms_timeouts", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_DPMS; i++) {
        config_setting_set_int_elem(setting, -1, conf.dpms_timeouts[ON_BATTERY][i]);
    }

    setting = config_setting_add(root, "ac_capture_timeouts", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_STATES; i++) {
        config_setting_set_int_elem(setting, -1, conf.timeout[ON_AC][i]);
    }

    setting = config_setting_add(root, "batt_capture_timeouts", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_STATES; i++) {
        config_setting_set_int_elem(setting, -1, conf.timeout[ON_BATTERY][i]);
    }

    setting = config_setting_add(root, "dimmer_timeouts", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_AC; i++) {
        config_setting_set_int_elem(setting, -1, conf.dimmer_timeout[i]);
    }

    setting = config_setting_add(root, "gamma_temp", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_STATES - 1; i++) {
        config_setting_set_int_elem(setting, -1, conf.temp[i]);
    }

    if(config_write_file(&cfg, config_file) != CONFIG_TRUE) {
        WARN("Failed to write new config to file.\n");
        r = -1;
    } else {
        INFO("New configuration successfully written to: %s\n", config_file);
    }
    config_destroy(&cfg);
    return r;
}
