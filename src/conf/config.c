#include <config.h>
#include <libconfig.h>

static void init_config_file(enum CONFIG file);

static char config_file[PATH_MAX + 1];

/*
 * Use getpwuid to get user home dir
 */
static void init_config_file(enum CONFIG file) {
    if (file == LOCAL) {
        if (getenv("XDG_CONFIG_HOME")) {
            snprintf(config_file, PATH_MAX, "%s/clight.conf", getenv("XDG_CONFIG_HOME"));
        } else {        
            snprintf(config_file, PATH_MAX, "%s/.config/clight.conf", getpwuid(getuid())->pw_dir);
        }
    } else {
        snprintf(config_file, PATH_MAX, "%s/clight.conf", CONFDIR);
    }
}

void read_config(enum CONFIG file) {
    config_t cfg;
    const char *videodev, *screendev, *sunrise, *sunset;
    
    init_config_file(file);
    if (access(config_file, F_OK) == -1) {
        return WARN("Config file %s not found.\n", config_file);
    }
    
    config_init(&cfg);
    if (config_read_file(&cfg, config_file) == CONFIG_TRUE) {
        config_lookup_int(&cfg, "frames", &conf.num_captures);
        config_lookup_int(&cfg, "no_smooth_backlight_transition", &conf.no_smooth_backlight);
        config_lookup_int(&cfg, "no_smooth_gamma_transition", &conf.no_smooth_gamma);
        config_lookup_int(&cfg, "no_smooth_dimmer_transition", &conf.no_smooth_dimmer);
        config_lookup_float(&cfg, "backlight_trans_step", &conf.backlight_trans_step);
        config_lookup_int(&cfg, "gamma_trans_step", &conf.gamma_trans_step);
        config_lookup_float(&cfg, "dimmer_trans_step", &conf.dimmer_trans_step);
        config_lookup_int(&cfg, "backlight_trans_timeout", &conf.backlight_trans_timeout);
        config_lookup_int(&cfg, "gamma_trans_timeout", &conf.gamma_trans_timeout);
        config_lookup_int(&cfg, "dimmer_trans_timeout", &conf.dimmer_trans_timeout);
        config_lookup_int(&cfg, "no_brightness", (int *)&modules[BRIGHTNESS].state);
        config_lookup_int(&cfg, "no_gamma", (int *)&modules[GAMMA].state);
        config_lookup_float(&cfg, "latitude", &conf.loc.lat);
        config_lookup_float(&cfg, "longitude", &conf.loc.lon);
        config_lookup_int(&cfg, "event_duration", &conf.event_duration);
        config_lookup_int(&cfg, "no_dimmer", (int *)&modules[DIMMER].state);
        config_lookup_float(&cfg, "dimmer_pct", &conf.dimmer_pct);
        config_lookup_int(&cfg, "no_dpms", (int *)&modules[DPMS].state);
        config_lookup_int(&cfg, "no_inhibit", (int *)&modules[INHIBIT].state);
        config_lookup_int(&cfg, "verbose", &conf.verbose);
        
        if (config_lookup_string(&cfg, "video_devname", &videodev) == CONFIG_TRUE) {
            strncpy(conf.dev_name, videodev, sizeof(conf.dev_name) - 1);
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
        
        /* Load regression points for brightness curve */
        if ((points = config_setting_get_member(root, "ac_brightness_regression_points"))) {
            if (config_setting_length(points) == SIZE_POINTS) {
                for (int i = 0; i < SIZE_POINTS; i++) {
                    conf.regression_points[ON_AC][i] = config_setting_get_float_elem(points, i);
                }
            } else {
                WARN("Wrong number of ac_brightness_regression_points array elements.\n");
            }
        }
        
        /* Load regression points for brightness curve */
        if ((points = config_setting_get_member(root, "batt_brightness_regression_points"))) {
            if (config_setting_length(points) == SIZE_POINTS) {
                for (int i = 0; i < SIZE_POINTS; i++) {
                    conf.regression_points[ON_BATTERY][i] = config_setting_get_float_elem(points, i);
                }
            } else {
                WARN("Wrong number of batt_brightness_regression_points array elements.\n");
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
    }
    config_destroy(&cfg);
}
