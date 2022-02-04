#include <libconfig.h>
#include "config.h"
#include "utils.h"

static void init_config_file(enum CONFIG file, char *filename);

static void load_generic_settings(config_t *cfg, conf_t *conf);
static void load_backlight_settings(config_t *cfg, bl_conf_t *conf);
static void load_sensor_settings(config_t *cfg, sensor_conf_t *conf);
static void load_override_settings(config_t *cfg, sensor_conf_t *conf);
static void load_kbd_settings(config_t *cfg, kbd_conf_t *conf);
static void load_gamma_settings(config_t *cfg, gamma_conf_t *conf);
static void load_day_settings(config_t *cfg, daytime_conf_t *conf);
static void load_dimmer_settings(config_t *cfg, dimmer_conf_t *conf);
static void load_dpms_settings(config_t *cfg, dpms_conf_t *conf);
static void load_screen_settings(config_t *cfg, screen_conf_t *conf);
static void load_inh_settings(config_t *cfg, inh_conf_t *conf);

static void store_generic_settings(config_t *cfg, conf_t *conf);
static void store_backlight_settings(config_t *cfg, bl_conf_t *conf);
static void store_sensors_settings(config_t *cfg, sensor_conf_t *conf);
static void store_override_settings(config_t *cfg, sensor_conf_t *conf);
static void store_kbd_settings(config_t *cfg, kbd_conf_t *conf);
static void store_gamma_settings(config_t *cfg, gamma_conf_t *conf);
static void store_daytime_settings(config_t *cfg, daytime_conf_t *conf);
static void store_dimmer_settings(config_t *cfg, dimmer_conf_t *conf);
static void store_dpms_settings(config_t *cfg, dpms_conf_t *conf);
static void store_screen_settings(config_t *cfg, screen_conf_t *conf);
static void store_inh_settings(config_t *cfg, inh_conf_t *conf);

static void init_config_file(enum CONFIG file, char *filename) {
    switch (file) {
        case LOCAL:
            if (getenv("XDG_CONFIG_HOME")) {
                snprintf(filename, PATH_MAX, "%s/clight.conf", getenv("XDG_CONFIG_HOME"));
            } else {
                snprintf(filename, PATH_MAX, "%s/.config/clight.conf", getpwuid(getuid())->pw_dir);
            }
            break;
        case GLOBAL:
            snprintf(filename, PATH_MAX, "%s/clight.conf", CONFDIR);
            break;
        default:
            return;
    }
}

#define X_CONF(tp, name, def) \
    _Generic((conf->name), \
        int: config_setting_lookup_int, \
        double: config_setting_lookup_float, \
        const char *: config_setting_lookup_string, \
        bool_to_int: config_setting_lookup_bool)(c, #name, &conf->name);

static void load_generic_settings(config_t *cfg, conf_t *conf) {
    config_setting_t *c = cfg->root;
    
    X_GENERIC_CONF
}

static void load_backlight_settings(config_t *cfg, bl_conf_t *conf) {
    config_setting_t *c = config_lookup(cfg, "backlight");
    if (!c) {
        return;
    }
    
    X_BL_CONF
            
    config_setting_t *timeouts;
    
    /* Load capture timeouts while on battery -> +1 because EVENT is exposed too */
    if ((timeouts = config_setting_get_member(c, "ac_timeouts"))) {
        if (config_setting_length(timeouts) == SIZE_STATES + 1) {
            for (int i = 0; i < SIZE_STATES + 1; i++) {
                conf->timeout[ON_AC][i] = config_setting_get_int_elem(timeouts, i);
            }
        } else {
            WARN("Wrong number of backlight 'ac_timeouts' array elements.\n");
        }
    }
    
    /* Load capture timeouts while on battery -> +1 because EVENT is exposed too */
    if ((timeouts = config_setting_get_member(c, "batt_timeouts"))) {
        if (config_setting_length(timeouts) == SIZE_STATES + 1) {
            for (int i = 0; i < SIZE_STATES + 1; i++) {
                conf->timeout[ON_BATTERY][i] = config_setting_get_int_elem(timeouts, i);
            }
        } else {
            WARN("Wrong number of backlight 'batt_timeouts' array elements.\n");
        }
    }
}

static void load_sensor_settings(config_t *cfg, sensor_conf_t *conf) {
    config_setting_t *c = config_lookup(cfg, "sensor");
    if (!c) {
        return;
    }
    
    X_SENS_CONF
    
    if (!is_string_empty(conf->devname)) {
        conf->devname = strdup(conf->devname);
    }
    if (!is_string_empty(conf->settings)) {
        conf->settings = strdup(conf->settings);
    }

    config_setting_t *captures, *points;
    /* Load num captures options */
    if ((captures = config_setting_get_member(c, "captures"))) {
        if (config_setting_length(captures) == SIZE_AC) {
            for (int i = 0; i < SIZE_AC; i++) {
                conf->num_captures[i] = config_setting_get_int_elem(captures, i);
            }
        } else {
            WARN("Wrong number of sensor 'captures' array elements.\n");
        }
    }

    /* Load regression points for ac backlight curve */
    int len;
    if ((points = config_setting_get_member(c, "ac_regression_points"))) {
        len = config_setting_length(points);
        if (len > 0 && len <= MAX_SIZE_POINTS) {
            conf->default_curve[ON_AC].num_points = len;
            for (int i = 0; i < len; i++) {
                conf->default_curve[ON_AC].points[i] = config_setting_get_float_elem(points, i);
            }
        } else {
            WARN("Wrong number of sensor 'ac_regression_points' array elements.\n");
        }
    }
    
    /* Load regression points for batt backlight curve */
    if ((points = config_setting_get_member(c, "batt_regression_points"))) {
        len = config_setting_length(points);
        if (len > 0 && len <= MAX_SIZE_POINTS) {
            conf->default_curve[ON_BATTERY].num_points = len;
            for (int i = 0; i < len; i++) {
                conf->default_curve[ON_BATTERY].points[i] = config_setting_get_float_elem(points, i);
            }
        } else {
            WARN("Wrong number of sensor 'batt_regression_points' array elements.\n");
        }
    }
}

static void load_override_settings(config_t *cfg, sensor_conf_t *sens_conf) {
    config_setting_t *override_group = config_lookup(cfg, "monitor_override");
    if (override_group) {
        int count = config_setting_length(override_group);
        for (int i = 0; i < count; i++) {
            config_setting_t *setting = config_setting_get_elem(override_group, i);
            
            curve_t *curve = calloc(SIZE_AC, sizeof(curve_t));
            bool error = false;
            const char *mon_id = NULL;
            if (config_setting_lookup_string(setting, "monitor_id", &mon_id) == CONFIG_TRUE && !is_string_empty(mon_id)) {
                config_setting_t *points;
                int len;
                
                /* Load regression points for ac backlight curve */
                if ((points = config_setting_get_member(setting, "ac_regression_points"))) {
                    len = config_setting_length(points);
                    if (len > 0 && len <= MAX_SIZE_POINTS) {
                        curve[ON_AC].num_points = len;
                        for (int i = 0; i < len; i++) {
                            curve[ON_AC].points[i] = config_setting_get_float_elem(points, i);
                        }
                    } else {
                        WARN("Wrong number of sensor 'ac_regression_points' array elements.\n");
                        error = true;
                    }
                } else {
                    error = true;
                }
                
                /* Load regression points for batt backlight curve */
                if (!error && (points = config_setting_get_member(setting, "batt_regression_points"))) {
                    len = config_setting_length(points);
                    if (len > 0 && len <= MAX_SIZE_POINTS) {
                        curve[ON_BATTERY].num_points = len;
                        for (int i = 0; i < len; i++) {
                            curve[ON_BATTERY].points[i] = config_setting_get_float_elem(points, i);
                        }
                    } else {
                        WARN("Wrong number of sensor 'batt_regression_points' array elements.\n");
                        error = true;
                    }
                } else {
                    error = true;
                }
            } else {
                error = true;
            }
            
            if (!error) {
                map_put(sens_conf->specific_curves, mon_id, curve);
            } else {
                free(curve);
            }
        }
    }
}

static void load_kbd_settings(config_t *cfg, kbd_conf_t *conf) {
    config_setting_t *c = config_lookup(cfg, "keyboard");
    if (!c) {
        return;
    }
    
    X_KBD_CONF

    /* Load timeouts */
    config_setting_t *timeouts, *points;
    if ((timeouts = config_setting_get_member(c, "timeouts"))) {
        if (config_setting_length(timeouts) == SIZE_AC) {
            for (int i = 0; i < SIZE_AC; i++) {
                conf->timeout[i] = config_setting_get_int_elem(timeouts, i);
            }
        } else {
            WARN("Wrong number of kbd 'timeouts' array elements.\n");
        }
    }
    
        /* Load regression points for ac keyboard curve */
    int len;
    if ((points = config_setting_get_member(c, "ac_regression_points"))) {
        len = config_setting_length(points);
        if (len > 0 && len <= MAX_SIZE_POINTS) {
            conf->curve[ON_AC].num_points = len;
            for (int i = 0; i < len; i++) {
                conf->curve[ON_AC].points[i] = config_setting_get_float_elem(points, i);
            }
        } else {
            WARN("Wrong number of keyboard 'ac_regression_points' array elements.\n");
        }
    }
    
    /* Load regression points for batt keyboard curve */
    if ((points = config_setting_get_member(c, "batt_regression_points"))) {
        len = config_setting_length(points);
        if (len > 0 && len <= MAX_SIZE_POINTS) {
            conf->curve[ON_BATTERY].num_points = len;
            for (int i = 0; i < len; i++) {
                conf->curve[ON_BATTERY].points[i] = config_setting_get_float_elem(points, i);
            }
        } else {
            WARN("Wrong number of keyboard 'batt_regression_points' array elements.\n");
        }
    }
}

static void load_gamma_settings(config_t *cfg, gamma_conf_t *conf) {
    config_setting_t *c = config_lookup(cfg, "gamma");
    if (!c) {
        return;
    }
    
    X_GAMMA_CONF

    config_setting_t *temp;
    if ((temp = config_setting_get_member(c, "temp"))) {
        if (config_setting_length(temp) == SIZE_STATES) {
            for (int i = 0; i < SIZE_STATES; i++) {
                conf->temp[i] = config_setting_get_int_elem(temp, i);
            }
        } else {
            WARN("Wrong number of gamma 'temp' array elements.\n");
        }
    }
}

static void load_day_settings(config_t *cfg, daytime_conf_t *conf) {
    config_setting_t *c = config_lookup(cfg, "daytime");
    if (!c) {
        return;
    }
    
    X_DAYTIME_CONF
    
    const char *sunrise, *sunset;
    config_setting_lookup_float(c, "latitude", &conf->loc.lat);
    config_setting_lookup_float(c, "longitude", &conf->loc.lon);
    config_setting_lookup_int(c, "sunrise_offset", &conf->events_os[SUNRISE]);
    config_setting_lookup_int(c, "sunset_offset", &conf->events_os[SUNSET]);
    
    if (config_setting_lookup_string(c, "sunrise", &sunrise) == CONFIG_TRUE) {
        strncpy(conf->day_events[SUNRISE], sunrise, sizeof(conf->day_events[SUNRISE]) - 1);
    }
    if (config_setting_lookup_string(c, "sunset", &sunset) == CONFIG_TRUE) {
        strncpy(conf->day_events[SUNSET], sunset, sizeof(conf->day_events[SUNSET]) - 1);
    }
}

static void load_dimmer_settings(config_t *cfg, dimmer_conf_t *conf) {
    config_setting_t *c = config_lookup(cfg, "dimmer");
    if (!c) {
        return;
    }

    X_DIMMER_CONF

    config_setting_t *points, *timeouts;
    /* Load no_smooth_dimmer options */
    if ((points = config_setting_get_member(c, "no_smooth_transition"))) {
        if (config_setting_length(points) == SIZE_DIM) {
            for (int i = 0; i < SIZE_DIM; i++) {
                conf->smooth[i].no_smooth = config_setting_get_float_elem(points, i);
            }
        } else {
            WARN("Wrong number of dimmer 'no_smooth_transition' array elements.\n");
        }
    }
    
    /* Load dimmer_trans_steps options */
    if ((points = config_setting_get_member(c, "trans_steps"))) {
        if (config_setting_length(points) == SIZE_DIM) {
            for (int i = 0; i < SIZE_DIM; i++) {
                conf->smooth[i].trans_step = config_setting_get_float_elem(points, i);
            }
        } else {
            WARN("Wrong number of dimmer 'trans_steps' array elements.\n");
        }
    }
    
    /* Load dimmer_trans_timeouts options */
    if ((points = config_setting_get_member(c, "trans_timeouts"))) {
        if (config_setting_length(points) == SIZE_DIM) {
            for (int i = 0; i < SIZE_DIM; i++) {
                conf->smooth[i].trans_timeout = config_setting_get_int_elem(points, i);
            }
        } else {
            WARN("Wrong number of dimmer 'trans_timeouts' array elements.\n");
        }
    }
    
    /* Load dimmer_trans_timeouts options */
    if ((points = config_setting_get_member(c, "trans_fixed"))) {
        if (config_setting_length(points) == SIZE_DIM) {
            for (int i = 0; i < SIZE_DIM; i++) {
                conf->smooth[i].trans_fixed = config_setting_get_int_elem(points, i);
            }
        } else {
            WARN("Wrong number of dimmer 'trans_fixed' array elements.\n");
        }
    }
    
    /* Load dimmer timeouts */
    if ((timeouts = config_setting_get_member(c, "timeouts"))) {
        if (config_setting_length(timeouts) == SIZE_AC) {
            for (int i = 0; i < SIZE_AC; i++) {
                conf->timeout[i] = config_setting_get_int_elem(timeouts, i);
            }
        } else {
            WARN("Wrong number of dimmer 'timeouts' array elements.\n");
        }
    }
}

static void load_dpms_settings(config_t *cfg, dpms_conf_t *conf) {
    config_setting_t *c = config_lookup(cfg, "dpms");
    if (!c) {
        return;
    }
        
    X_DPMS_CONF
    
    config_setting_t *timeouts;
    /* Load dpms timeouts */
    if ((timeouts = config_setting_get_member(c, "timeouts"))) {
        if (config_setting_length(timeouts) == SIZE_AC) {
            for (int i = 0; i < SIZE_AC; i++) {
                conf->timeout[i] = config_setting_get_int_elem(timeouts, i);
            }
        } else {
            WARN("Wrong number of dpms 'timeouts' array elements.\n");
        }
    }
}

static void load_screen_settings(config_t *cfg, screen_conf_t *conf) {
    config_setting_t *c = config_lookup(cfg, "screen");
    if (!c) {
        return;
    }
        
    X_SCREEN_CONF
        
    config_setting_t *timeouts;
    if ((timeouts = config_setting_get_member(c, "timeouts"))) {
        if (config_setting_length(timeouts) == SIZE_AC) {
            for (int i = 0; i < SIZE_AC; i++) {
                conf->timeout[i] = config_setting_get_int_elem(timeouts, i);
            }
        } else {
            WARN("Wrong number of screen 'timeouts' array elements.\n");
        }
    }
}

static void load_inh_settings(config_t *cfg, inh_conf_t *conf) {
    config_setting_t *c = config_lookup(cfg, "inhibit");
    if (!c) {
        return;
    }
    
    X_INH_CONF
}

#undef X_CONF

int read_config(enum CONFIG file, char *config_file) {
    int r = 0;
    config_t cfg;
    
    if (is_string_empty(config_file)) {
        init_config_file(file, config_file);
    }
    if (access(config_file, F_OK) == -1) {
        WARN("Config file %s not found.\n", config_file);
        return -1;
    }
    
    config_init(&cfg);
    if (config_read_file(&cfg, config_file) == CONFIG_TRUE) {
        load_generic_settings(&cfg, &conf);
        load_backlight_settings(&cfg, &conf.bl_conf);
        load_sensor_settings(&cfg, &conf.sens_conf);
        load_override_settings(&cfg, &conf.sens_conf);
        load_kbd_settings(&cfg, &conf.kbd_conf);
        load_gamma_settings(&cfg, &conf.gamma_conf);
        load_day_settings(&cfg, &conf.day_conf);
        load_dimmer_settings(&cfg, &conf.dim_conf);
        load_dpms_settings(&cfg, &conf.dpms_conf);
        load_screen_settings(&cfg, &conf.screen_conf);
        load_inh_settings(&cfg, &conf.inh_conf);
    } else {
        WARN("Config file: %s at line %d.\n",
             config_error_text(&cfg),
             config_error_line(&cfg));
        r = -1;
    }
    config_destroy(&cfg);
    return r;
}

#define X_CONF(tp, name, def) \
    do { \
    config_setting_t *setting = config_setting_add(c, #name, \
    _Generic((conf->name), \
        int: CONFIG_TYPE_INT, \
        double: CONFIG_TYPE_FLOAT, \
        const char *: CONFIG_TYPE_STRING, \
        bool_to_int: CONFIG_TYPE_BOOL)); \
    \
    _Generic((conf->name), \
        int: config_setting_set_int, \
        double: config_setting_set_float, \
        const char *: config_setting_set_string, \
        bool_to_int: config_setting_set_bool)(setting, conf->name); \
    } while(0)

static void store_generic_settings(config_t *cfg, conf_t *conf) {
    config_setting_t *c = cfg->root;

    X_GENERIC_CONF
}

static void store_backlight_settings(config_t *cfg, bl_conf_t *conf) {
    config_setting_t *c = config_setting_add(cfg->root, "backlight", CONFIG_TYPE_GROUP);
    
    X_BL_CONF
    
    config_setting_t *setting = config_setting_add(c, "ac_timeouts", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_STATES + 1; i++) {
        config_setting_set_int_elem(setting, -1, conf->timeout[ON_AC][i]);
    }
    
    setting = config_setting_add(c, "batt_timeouts", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_STATES + 1; i++) {
        config_setting_set_int_elem(setting, -1, conf->timeout[ON_BATTERY][i]);
    }
}

static void store_sensors_settings(config_t *cfg, sensor_conf_t *conf) {
    config_setting_t *c = config_setting_add(cfg->root, "sensor", CONFIG_TYPE_GROUP);
    
    X_SENS_CONF
    
    config_setting_t *setting = config_setting_add(c, "captures", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_AC; i++) {
        config_setting_set_int_elem(setting, -1, conf->num_captures[i]);
    }
        
    /* -1 here below means append to end of array */
    setting = config_setting_add(c, "ac_regression_points", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < conf->default_curve[ON_AC].num_points; i++) {
        config_setting_set_float_elem(setting, -1, conf->default_curve[ON_AC].points[i]);
    }
            
    setting = config_setting_add(c, "batt_regression_points", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < conf->default_curve[ON_BATTERY].num_points; i++) {
        config_setting_set_float_elem(setting, -1, conf->default_curve[ON_BATTERY].points[i]);
    }
}

static void store_override_settings(config_t *cfg, sensor_conf_t *conf) {
    config_setting_t *override = config_setting_add(cfg->root, "monitor_override", CONFIG_TYPE_LIST);
    for (map_itr_t *itr = map_itr_new(conf->specific_curves); itr; itr = map_itr_next(itr)) {
        curve_t *c = map_itr_get_data(itr);
        const char *key = map_itr_get_key(itr);
        
        config_setting_t *monitor = config_setting_add(override, NULL, CONFIG_TYPE_GROUP);
        
        config_setting_t *setting = config_setting_add(monitor, "monitor_id", CONFIG_TYPE_STRING);
        config_setting_set_string(setting, key);
        
        /* -1 here below means append to end of array */
        setting = config_setting_add(monitor, "ac_regression_points", CONFIG_TYPE_ARRAY);
        for (int i = 0; i < c[ON_AC].num_points; i++) {
            config_setting_set_float_elem(setting, -1, c[ON_AC].points[i]);
        }
                
        setting = config_setting_add(monitor, "batt_regression_points", CONFIG_TYPE_ARRAY);
        for (int i = 0; i < c[ON_BATTERY].num_points; i++) {
            config_setting_set_float_elem(setting, -1, c[ON_BATTERY].points[i]);
        }
    }
}

static void store_kbd_settings(config_t *cfg, kbd_conf_t *conf) {
    config_setting_t *c = config_setting_add(cfg->root, "keyboard", CONFIG_TYPE_GROUP);
    
    X_KBD_CONF
    
    config_setting_t *setting = NULL;
    /* -1 here below means append to end of array */
    setting = config_setting_add(c, "ac_regression_points", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < conf->curve[ON_AC].num_points; i++) {
        config_setting_set_float_elem(setting, -1, conf->curve[ON_AC].points[i]);
    }
                
    setting = config_setting_add(c, "batt_regression_points", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < conf->curve[ON_BATTERY].num_points; i++) {
        config_setting_set_float_elem(setting, -1, conf->curve[ON_BATTERY].points[i]);
    }
    
    setting = config_setting_add(c, "timeouts", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_AC; i++) {
        config_setting_set_int_elem(setting, -1, conf->timeout[i]);
    }
}

static void store_gamma_settings(config_t *cfg, gamma_conf_t *conf) {
    config_setting_t *c = config_setting_add(cfg->root, "gamma", CONFIG_TYPE_GROUP);
    
    X_GAMMA_CONF
    
    config_setting_t *setting = config_setting_add(c, "temp", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_STATES; i++) {
        config_setting_set_int_elem(setting, -1, conf->temp[i]);
    }
}

static void store_daytime_settings(config_t *cfg, daytime_conf_t *conf) {
    config_setting_t *c = config_setting_add(cfg->root, "daytime", CONFIG_TYPE_GROUP);
    
    X_DAYTIME_CONF
    
    config_setting_t *setting = NULL;
    if (conf->loc.lat != LAT_UNDEFINED && conf->loc.lon != LON_UNDEFINED) {
        setting = config_setting_add(c, "latitude", CONFIG_TYPE_FLOAT);
        config_setting_set_float(setting, conf->loc.lat);
        setting = config_setting_add(c, "longitude", CONFIG_TYPE_FLOAT);
        config_setting_set_float(setting, conf->loc.lon);
    }
    
    setting = config_setting_add(c, "sunrise_offset", CONFIG_TYPE_INT);
    config_setting_set_int(setting, conf->events_os[SUNRISE]);
    
    setting = config_setting_add(c, "sunset_offset", CONFIG_TYPE_INT);
    config_setting_set_int(setting, conf->events_os[SUNSET]);
    
    setting = config_setting_add(c, "sunrise", CONFIG_TYPE_STRING);
    config_setting_set_string(setting, conf->day_events[SUNRISE]);
    
    setting = config_setting_add(c, "sunset", CONFIG_TYPE_STRING);
    config_setting_set_string(setting, conf->day_events[SUNSET]);
}

static void store_dimmer_settings(config_t *cfg, dimmer_conf_t *conf) {
    config_setting_t *c = config_setting_add(cfg->root, "dimmer", CONFIG_TYPE_GROUP);
    
    X_DIMMER_CONF
    
    config_setting_t *setting = config_setting_add(c, "no_smooth_transition", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_DIM; i++) {
        config_setting_set_bool_elem(setting, -1, conf->smooth[i].no_smooth);
    }
    
    setting = config_setting_add(c, "trans_steps", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_DIM; i++) {
        config_setting_set_float_elem(setting, -1, conf->smooth[i].trans_step);
    }
    
    setting = config_setting_add(c, "trans_timeouts", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_DIM; i++) {
        config_setting_set_int_elem(setting, -1, conf->smooth[i].trans_timeout);
    }
    
    setting = config_setting_add(c, "trans_fixed", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_DIM; i++) {
        config_setting_set_int_elem(setting, -1, conf->smooth[i].trans_fixed);
    }
    
    setting = config_setting_add(c, "timeouts", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_AC; i++) {
        config_setting_set_int_elem(setting, -1, conf->timeout[i]);
    }
}

static void store_dpms_settings(config_t *cfg, dpms_conf_t *conf) {
    config_setting_t *c = config_setting_add(cfg->root, "dpms", CONFIG_TYPE_GROUP);
    
    X_DPMS_CONF
    
    config_setting_t *setting = config_setting_add(c, "timeouts", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_AC; i++) {
        config_setting_set_int_elem(setting, -1, conf->timeout[i]);
    }
}

static void store_screen_settings(config_t *cfg, screen_conf_t *conf) {
    config_setting_t *c = config_setting_add(cfg->root, "screen", CONFIG_TYPE_GROUP);
    
    X_SCREEN_CONF
    
    config_setting_t *setting = config_setting_add(c, "timeouts", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_AC; i++) {
        config_setting_set_int_elem(setting, -1, conf->timeout[i]);
    }
}

static void store_inh_settings(config_t *cfg, inh_conf_t *conf) {
    config_setting_t *c = config_setting_add(cfg->root, "inhibit", CONFIG_TYPE_GROUP);
    
    X_INH_CONF
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
    
    store_generic_settings(&cfg, &conf);
    store_backlight_settings(&cfg, &conf.bl_conf);
    store_sensors_settings(&cfg, &conf.sens_conf);
    store_override_settings(&cfg, &conf.sens_conf);
    store_kbd_settings(&cfg, &conf.kbd_conf);
    store_gamma_settings(&cfg, &conf.gamma_conf);
    store_daytime_settings(&cfg, &conf.day_conf);
    store_dimmer_settings(&cfg, &conf.dim_conf);
    store_dpms_settings(&cfg, &conf.dpms_conf);
    store_screen_settings(&cfg, &conf.screen_conf);
    store_inh_settings(&cfg, &conf.inh_conf);
    
    if (config_write_file(&cfg, config_file) != CONFIG_TRUE) {
        WARN("Failed to write new config to file.\n");
        r = -1;
    } else {
        INFO("New configuration successfully written to: %s\n", config_file);
    }
    config_destroy(&cfg);
    return r;
}

#undef X_CONF
