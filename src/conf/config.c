#include <libconfig.h>
#include "config.h"
#include "utils.h"

static void init_config_file(enum CONFIG file, char *filename);

static void load_backlight_settings(config_t *cfg, bl_conf_t *bl_conf);
static void load_sensor_settings(config_t *cfg, sensor_conf_t *sens_conf);
static void load_override_settings(config_t *cfg, sensor_conf_t *sens_conf);
static void load_kbd_settings(config_t *cfg, kbd_conf_t *kbd_conf);
static void load_gamma_settings(config_t *cfg, gamma_conf_t *gamma_conf);
static void load_day_settings(config_t *cfg, daytime_conf_t *day_conf);
static void load_dimmer_settings(config_t *cfg, dimmer_conf_t *dim_conf);
static void load_dpms_settings(config_t *cfg, dpms_conf_t *dpms_conf);
static void load_screen_settings(config_t *cfg, screen_conf_t *screen_conf);
static void load_inh_settings(config_t *cfg, inh_conf_t *inh_conf);

static void store_backlight_settings(config_t *cfg, bl_conf_t *bl_conf);
static void store_sensors_settings(config_t *cfg, sensor_conf_t *sens_conf);
static void store_override_settings(config_t *cfg, sensor_conf_t *sens_conf);
static void store_kbd_settings(config_t *cfg, kbd_conf_t *kbd_conf);
static void store_gamma_settings(config_t *cfg, gamma_conf_t *gamma_conf);
static void store_daytime_settings(config_t *cfg, daytime_conf_t *day_conf);
static void store_dimmer_settings(config_t *cfg, dimmer_conf_t *dim_conf);
static void store_dpms_settings(config_t *cfg, dpms_conf_t *dpms_conf);
static void store_screen_settings(config_t *cfg, screen_conf_t *screen_conf);
static void store_inh_settings(config_t *cfg, inh_conf_t *inh_conf);

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

static void load_backlight_settings(config_t *cfg, bl_conf_t *bl_conf) {
    config_setting_t *bl = config_lookup(cfg, "backlight");
    if (bl) {
        config_setting_lookup_bool(bl, "disabled", &bl_conf->disabled);
        config_setting_lookup_bool(bl, "restore_on_exit", &bl_conf->restore);
        config_setting_lookup_bool(bl, "no_smooth_transition", &bl_conf->smooth.no_smooth);
        config_setting_lookup_float(bl, "trans_step", &bl_conf->smooth.trans_step);
        config_setting_lookup_int(bl, "trans_timeout", &bl_conf->smooth.trans_timeout);
        config_setting_lookup_int(bl, "trans_fixed", &bl_conf->smooth.trans_fixed);
        config_setting_lookup_float(bl, "shutter_threshold", &bl_conf->shutter_threshold);
        config_setting_lookup_bool(bl, "no_auto_calibration", &bl_conf->no_auto_calib);
        config_setting_lookup_bool(bl, "pause_on_lid_closed", &bl_conf->pause_on_lid_closed);
        config_setting_lookup_bool(bl, "capture_on_lid_opened", &bl_conf->capture_on_lid_opened);
        
        config_setting_t *timeouts;
        
        /* Load capture timeouts while on battery -> +1 because EVENT is exposed too */
        if ((timeouts = config_setting_get_member(bl, "ac_timeouts"))) {
            if (config_setting_length(timeouts) == SIZE_STATES + 1) {
                for (int i = 0; i < SIZE_STATES + 1; i++) {
                    bl_conf->timeout[ON_AC][i] = config_setting_get_int_elem(timeouts, i);
                }
            } else {
                WARN("Wrong number of backlight 'ac_timeouts' array elements.\n");
            }
        }
        
        /* Load capture timeouts while on battery -> +1 because EVENT is exposed too */
        if ((timeouts = config_setting_get_member(bl, "batt_timeouts"))) {
            if (config_setting_length(timeouts) == SIZE_STATES + 1) {
                for (int i = 0; i < SIZE_STATES + 1; i++) {
                    bl_conf->timeout[ON_BATTERY][i] = config_setting_get_int_elem(timeouts, i);
                }
            } else {
                WARN("Wrong number of backlight 'batt_timeouts' array elements.\n");
            }
        }
    }
}

static void load_sensor_settings(config_t *cfg, sensor_conf_t *sens_conf) {
    config_setting_t *sens_group = config_lookup(cfg, "sensor");
    if (sens_group) {
        const char *sensor_dev = NULL, *sensor_settings = NULL;
        
        if (config_setting_lookup_string(sens_group, "devname", &sensor_dev) == CONFIG_TRUE && !is_string_empty(sensor_dev)) {
            sens_conf->dev_name = strdup(sensor_dev);
        }
    
        if (config_setting_lookup_string(sens_group, "settings", &sensor_settings) == CONFIG_TRUE && !is_string_empty(sensor_settings)) {
            sens_conf->dev_opts = strdup(sensor_settings);
        }
        
        config_setting_t *captures, *points;
        /* Load num captures options */
        if ((captures = config_setting_get_member(sens_group, "captures"))) {
            if (config_setting_length(captures) == SIZE_AC) {
                for (int i = 0; i < SIZE_AC; i++) {
                    sens_conf->num_captures[i] = config_setting_get_int_elem(captures, i);
                }
            } else {
                WARN("Wrong number of sensor 'captures' array elements.\n");
            }
        }
    
        /* Load regression points for ac backlight curve */
        int len;
        if ((points = config_setting_get_member(sens_group, "ac_regression_points"))) {
            len = config_setting_length(points);
            if (len > 0 && len <= MAX_SIZE_POINTS) {
                sens_conf->default_curve[ON_AC].num_points = len;
                for (int i = 0; i < len; i++) {
                   sens_conf->default_curve[ON_AC].points[i] = config_setting_get_float_elem(points, i);
                }
            } else {
                WARN("Wrong number of sensor 'ac_regression_points' array elements.\n");
            }
        }
        
        /* Load regression points for batt backlight curve */
        if ((points = config_setting_get_member(sens_group, "batt_regression_points"))) {
            len = config_setting_length(points);
            if (len > 0 && len <= MAX_SIZE_POINTS) {
                sens_conf->default_curve[ON_BATTERY].num_points = len;
                for (int i = 0; i < len; i++) {
                    sens_conf->default_curve[ON_BATTERY].points[i] = config_setting_get_float_elem(points, i);
                }
            } else {
                WARN("Wrong number of sensor 'batt_regression_points' array elements.\n");
            }
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

static void load_kbd_settings(config_t *cfg, kbd_conf_t *kbd_conf) {
    config_setting_t *kbd = config_lookup(cfg, "keyboard");
    if (kbd) {
        config_setting_lookup_bool(kbd, "disabled", &kbd_conf->disabled);
        
        /* Load timeouts */
        config_setting_t *timeouts, *points;
        if ((timeouts = config_setting_get_member(kbd, "timeouts"))) {
            if (config_setting_length(timeouts) == SIZE_AC) {
                for (int i = 0; i < SIZE_AC; i++) {
                    kbd_conf->timeout[i] = config_setting_get_int_elem(timeouts, i);
                }
            } else {
                WARN("Wrong number of kbd 'timeouts' array elements.\n");
            }
        }
        
         /* Load regression points for ac keyboard curve */
        int len;
        if ((points = config_setting_get_member(kbd, "ac_regression_points"))) {
            len = config_setting_length(points);
            if (len > 0 && len <= MAX_SIZE_POINTS) {
                kbd_conf->curve[ON_AC].num_points = len;
                for (int i = 0; i < len; i++) {
                   kbd_conf->curve[ON_AC].points[i] = config_setting_get_float_elem(points, i);
                }
            } else {
                WARN("Wrong number of keyboard 'ac_regression_points' array elements.\n");
            }
        }
        
        /* Load regression points for batt keyboard curve */
        if ((points = config_setting_get_member(kbd, "batt_regression_points"))) {
            len = config_setting_length(points);
            if (len > 0 && len <= MAX_SIZE_POINTS) {
                kbd_conf->curve[ON_BATTERY].num_points = len;
                for (int i = 0; i < len; i++) {
                    kbd_conf->curve[ON_BATTERY].points[i] = config_setting_get_float_elem(points, i);
                }
            } else {
                WARN("Wrong number of keyboard 'batt_regression_points' array elements.\n");
            }
        }
    }
}

static void load_gamma_settings(config_t *cfg, gamma_conf_t *gamma_conf) {
    config_setting_t *gamma = config_lookup(cfg, "gamma");
    if (gamma) {        
        config_setting_lookup_bool(gamma, "disabled", &gamma_conf->disabled);
        config_setting_lookup_bool(gamma, "restore_on_exit", &gamma_conf->restore);
        config_setting_lookup_bool(gamma, "no_smooth_transition", &gamma_conf->no_smooth);
        config_setting_lookup_int(gamma, "trans_step", &gamma_conf->trans_step);
        config_setting_lookup_int(gamma, "trans_timeout", &gamma_conf->trans_timeout);
        config_setting_lookup_bool(gamma, "long_transition", &gamma_conf->long_transition);
        config_setting_lookup_bool(gamma, "ambient_gamma", &gamma_conf->ambient_gamma);
        
        if ((gamma = config_setting_get_member(gamma, "temp"))) {
            if (config_setting_length(gamma) == SIZE_STATES) {
                for (int i = 0; i < SIZE_STATES; i++) {
                    gamma_conf->temp[i] = config_setting_get_int_elem(gamma, i);
                }
            } else {
                WARN("Wrong number of gamma 'temp' array elements.\n");
            }
        }
    }
}

static void load_day_settings(config_t *cfg, daytime_conf_t *day_conf) {
    config_setting_t *daytime = config_lookup(cfg, "daytime");
    if (daytime) {
        const char *sunrise, *sunset;
        
        config_setting_lookup_float(daytime, "latitude", &day_conf->loc.lat);
        config_setting_lookup_float(daytime, "longitude", &day_conf->loc.lon);
        config_setting_lookup_int(daytime, "event_duration", &day_conf->event_duration);
        config_setting_lookup_int(daytime, "sunrise_offset", &day_conf->events_os[SUNRISE]);
        config_setting_lookup_int(daytime, "sunset_offset", &day_conf->events_os[SUNSET]);
        
        if (config_setting_lookup_string(daytime, "sunrise", &sunrise) == CONFIG_TRUE) {
            strncpy(day_conf->day_events[SUNRISE], sunrise, sizeof(day_conf->day_events[SUNRISE]) - 1);
        }
        if (config_setting_lookup_string(daytime, "sunset", &sunset) == CONFIG_TRUE) {
            strncpy(day_conf->day_events[SUNSET], sunset, sizeof(day_conf->day_events[SUNSET]) - 1);
        }
    }
}

static void load_dimmer_settings(config_t *cfg, dimmer_conf_t *dim_conf) {
    config_setting_t *dim = config_lookup(cfg, "dimmer");
    if (dim) {
        config_setting_lookup_bool(dim, "disabled", &dim_conf->disabled);
        config_setting_lookup_float(dim, "dimmed_pct", &dim_conf->dimmed_pct);
         
        config_setting_t *points, *timeouts;
        /* Load no_smooth_dimmer options */
        if ((points = config_setting_get_member(dim, "no_smooth_transition"))) {
            if (config_setting_length(points) == SIZE_DIM) {
                for (int i = 0; i < SIZE_DIM; i++) {
                    dim_conf->smooth[i].no_smooth = config_setting_get_float_elem(points, i);
                }
            } else {
                WARN("Wrong number of dimmer 'no_smooth_transition' array elements.\n");
            }
        }
        
        /* Load dimmer_trans_steps options */
        if ((points = config_setting_get_member(dim, "trans_steps"))) {
            if (config_setting_length(points) == SIZE_DIM) {
                for (int i = 0; i < SIZE_DIM; i++) {
                    dim_conf->smooth[i].trans_step = config_setting_get_float_elem(points, i);
                }
            } else {
                WARN("Wrong number of dimmer 'trans_steps' array elements.\n");
            }
        }
        
        /* Load dimmer_trans_timeouts options */
        if ((points = config_setting_get_member(dim, "trans_timeouts"))) {
            if (config_setting_length(points) == SIZE_DIM) {
                for (int i = 0; i < SIZE_DIM; i++) {
                    dim_conf->smooth[i].trans_timeout = config_setting_get_int_elem(points, i);
                }
            } else {
                WARN("Wrong number of dimmer 'trans_timeouts' array elements.\n");
            }
        }
        
        /* Load dimmer_trans_timeouts options */
        if ((points = config_setting_get_member(dim, "trans_fixed"))) {
            if (config_setting_length(points) == SIZE_DIM) {
                for (int i = 0; i < SIZE_DIM; i++) {
                    dim_conf->smooth[i].trans_fixed = config_setting_get_int_elem(points, i);
                }
            } else {
                WARN("Wrong number of dimmer 'trans_fixed' array elements.\n");
            }
        }
        
        /* Load dimmer timeouts */
        if ((timeouts = config_setting_get_member(dim, "timeouts"))) {
            if (config_setting_length(timeouts) == SIZE_AC) {
                for (int i = 0; i < SIZE_AC; i++) {
                    dim_conf->timeout[i] = config_setting_get_int_elem(timeouts, i);
                }
            } else {
                WARN("Wrong number of dimmer 'timeouts' array elements.\n");
            }
        }
    }
}

static void load_dpms_settings(config_t *cfg, dpms_conf_t *dpms_conf) {
    config_setting_t *dpms = config_lookup(cfg, "dpms");
    if (dpms) {
        config_setting_lookup_bool(dpms, "disabled", &dpms_conf->disabled);
        
        config_setting_t *timeouts;
        
        /* Load dpms timeouts */
        if ((timeouts = config_setting_get_member(dpms, "timeouts"))) {
            if (config_setting_length(timeouts) == SIZE_AC) {
                for (int i = 0; i < SIZE_AC; i++) {
                    dpms_conf->timeout[i] = config_setting_get_int_elem(timeouts, i);
                }
            } else {
                WARN("Wrong number of dpms 'timeouts' array elements.\n");
            }
        }
    }
}

static void load_screen_settings(config_t *cfg, screen_conf_t *screen_conf) {
    config_setting_t *screen = config_lookup(cfg, "screen");
    if (screen) {
        config_setting_lookup_bool(screen, "disabled", &screen_conf->disabled);
        config_setting_lookup_float(screen, "contrib", &screen_conf->contrib);
        config_setting_t *timeouts;
        if ((timeouts = config_setting_get_member(screen, "timeouts"))) {
            if (config_setting_length(timeouts) == SIZE_AC) {
                for (int i = 0; i < SIZE_AC; i++) {
                    screen_conf->timeout[i] = config_setting_get_int_elem(timeouts, i);
                }
            } else {
                WARN("Wrong number of screen 'timeouts' array elements.\n");
            }
        }
    }
}

static void load_inh_settings(config_t *cfg, inh_conf_t *inh_conf) {
    config_setting_t *kbd = config_lookup(cfg, "inhibit");
    if (kbd) {
        config_setting_lookup_bool(kbd, "disabled", &inh_conf->disabled);
        config_setting_lookup_bool(kbd, "inhibit_docked", &inh_conf->inhibit_docked);
        config_setting_lookup_bool(kbd, "inhibit_pm", &inh_conf->inhibit_pm);
        config_setting_lookup_bool(kbd, "inhibit_bl", &inh_conf->inhibit_bl);
    }
}

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

    DEBUG("Reading config file: %s\n", config_file);

    config_init(&cfg);
    if (config_read_file(&cfg, config_file) == CONFIG_TRUE) {
        config_lookup_bool(&cfg, "verbose", &conf.verbose);
        config_lookup_int(&cfg, "resumedelay", &conf.resumedelay);
        
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

static void store_backlight_settings(config_t *cfg, bl_conf_t *bl_conf) {
    config_setting_t *bl = config_setting_add(cfg->root, "backlight", CONFIG_TYPE_GROUP);
    
    config_setting_t *setting = config_setting_add(bl, "disabled", CONFIG_TYPE_BOOL);
    config_setting_set_bool(setting, bl_conf->disabled);
    
    setting = config_setting_add(bl, "restore_on_exit", CONFIG_TYPE_BOOL);
    config_setting_set_bool(setting, bl_conf->restore);
    
    setting = config_setting_add(bl, "no_smooth_transition", CONFIG_TYPE_BOOL);
    config_setting_set_bool(setting, bl_conf->smooth.no_smooth);
    
    setting = config_setting_add(bl, "trans_step", CONFIG_TYPE_FLOAT);
    config_setting_set_float(setting, bl_conf->smooth.trans_step);
    
    setting = config_setting_add(bl, "trans_timeout", CONFIG_TYPE_INT);
    config_setting_set_int(setting, bl_conf->smooth.trans_timeout);
    
    setting = config_setting_add(bl, "trans_fixed", CONFIG_TYPE_INT);
    config_setting_set_int(setting, bl_conf->smooth.trans_fixed);
    
    setting = config_setting_add(bl, "no_auto_calibration", CONFIG_TYPE_BOOL);
    config_setting_set_bool(setting, bl_conf->no_auto_calib);
    
    setting = config_setting_add(bl, "pause_on_lid_closed", CONFIG_TYPE_BOOL);
    config_setting_set_bool(setting, bl_conf->pause_on_lid_closed);
    
    setting = config_setting_add(bl, "capture_on_lid_opened", CONFIG_TYPE_BOOL);
    config_setting_set_bool(setting, bl_conf->capture_on_lid_opened);

    setting = config_setting_add(bl, "shutter_threshold", CONFIG_TYPE_FLOAT);
    config_setting_set_float(setting, bl_conf->shutter_threshold);
    
    setting = config_setting_add(bl, "ac_timeouts", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_STATES + 1; i++) {
        config_setting_set_int_elem(setting, -1, bl_conf->timeout[ON_AC][i]);
    }
    
    setting = config_setting_add(bl, "batt_timeouts", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_STATES + 1; i++) {
        config_setting_set_int_elem(setting, -1, bl_conf->timeout[ON_BATTERY][i]);
    }
}

static void store_sensors_settings(config_t *cfg, sensor_conf_t *sens_conf) {
    config_setting_t *sensor = config_setting_add(cfg->root, "sensor", CONFIG_TYPE_GROUP);
    config_setting_t *setting = config_setting_add(sensor, "captures", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_AC; i++) {
        config_setting_set_int_elem(setting, -1, sens_conf->num_captures[i]);
    }
    
    if (sens_conf->dev_name) {
        setting = config_setting_add(sensor, "devname", CONFIG_TYPE_STRING);
        config_setting_set_string(setting, sens_conf->dev_name); 
    }

    if (sens_conf->dev_opts) {
        setting = config_setting_add(sensor, "settings", CONFIG_TYPE_STRING);
        config_setting_set_string(setting, sens_conf->dev_opts);
    }
        
    /* -1 here below means append to end of array */
    setting = config_setting_add(sensor, "ac_regression_points", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < sens_conf->default_curve[ON_AC].num_points; i++) {
        config_setting_set_float_elem(setting, -1, sens_conf->default_curve[ON_AC].points[i]);
    }
            
    setting = config_setting_add(sensor, "batt_regression_points", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < sens_conf->default_curve[ON_BATTERY].num_points; i++) {
        config_setting_set_float_elem(setting, -1, sens_conf->default_curve[ON_BATTERY].points[i]);
    }
}

static void store_override_settings(config_t *cfg, sensor_conf_t *sens_conf) {
    config_setting_t *override = config_setting_add(cfg->root, "monitor_override", CONFIG_TYPE_LIST);
    for (map_itr_t *itr = map_itr_new(conf.sens_conf.specific_curves); itr; itr = map_itr_next(itr)) {
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

static void store_kbd_settings(config_t *cfg, kbd_conf_t *kbd_conf) {
    config_setting_t *kbd = config_setting_add(cfg->root, "keyboard", CONFIG_TYPE_GROUP);
    
    config_setting_t *setting = config_setting_add(kbd, "disabled", CONFIG_TYPE_BOOL);
    config_setting_set_bool(setting, kbd_conf->disabled);
    
    /* -1 here below means append to end of array */
    setting = config_setting_add(kbd, "ac_regression_points", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < kbd_conf->curve[ON_AC].num_points; i++) {
        config_setting_set_float_elem(setting, -1, kbd_conf->curve[ON_AC].points[i]);
    }
                
    setting = config_setting_add(kbd, "batt_regression_points", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < kbd_conf->curve[ON_BATTERY].num_points; i++) {
        config_setting_set_float_elem(setting, -1, kbd_conf->curve[ON_BATTERY].points[i]);
    }
    
    setting = config_setting_add(kbd, "timeouts", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_AC; i++) {
        config_setting_set_int_elem(setting, -1, kbd_conf->timeout[i]);
    }
}

static void store_gamma_settings(config_t *cfg, gamma_conf_t *gamma_conf) {
    config_setting_t *gamma = config_setting_add(cfg->root, "gamma", CONFIG_TYPE_GROUP);
    
    config_setting_t *setting = config_setting_add(gamma, "disabled", CONFIG_TYPE_BOOL);
    config_setting_set_bool(setting, gamma_conf->disabled);
    
    setting = config_setting_add(gamma, "restore_on_exit", CONFIG_TYPE_BOOL);
    config_setting_set_bool(setting, gamma_conf->restore);
    
    setting = config_setting_add(gamma, "no_smooth_transition", CONFIG_TYPE_BOOL);
    config_setting_set_bool(setting, gamma_conf->no_smooth);
    
    setting = config_setting_add(gamma, "trans_step", CONFIG_TYPE_INT);
    config_setting_set_int(setting, gamma_conf->trans_step);
    
    setting = config_setting_add(gamma, "trans_timeout", CONFIG_TYPE_INT);
    config_setting_set_int(setting, gamma_conf->trans_timeout);
    
    setting = config_setting_add(gamma, "long_transition", CONFIG_TYPE_BOOL);
    config_setting_set_bool(setting, gamma_conf->long_transition);
    
    setting = config_setting_add(gamma, "ambient_gamma", CONFIG_TYPE_BOOL);
    config_setting_set_bool(setting, gamma_conf->ambient_gamma);
    
    setting = config_setting_add(gamma, "temp", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_STATES; i++) {
        config_setting_set_int_elem(setting, -1, gamma_conf->temp[i]);
    }
}

static void store_daytime_settings(config_t *cfg, daytime_conf_t *day_conf) {
    config_setting_t *daytime = config_setting_add(cfg->root, "daytime", CONFIG_TYPE_GROUP);
    config_setting_t *setting;
    
    if (day_conf->loc.lat != LAT_UNDEFINED && day_conf->loc.lon != LON_UNDEFINED) {
        setting = config_setting_add(daytime, "latitude", CONFIG_TYPE_FLOAT);
        config_setting_set_float(setting, day_conf->loc.lat);
        setting = config_setting_add(daytime, "longitude", CONFIG_TYPE_FLOAT);
        config_setting_set_float(setting, day_conf->loc.lon);
    }
    
    setting = config_setting_add(daytime, "event_duration", CONFIG_TYPE_INT);
    config_setting_set_int(setting, day_conf->event_duration);
    
    setting = config_setting_add(daytime, "sunrise_offset", CONFIG_TYPE_INT);
    config_setting_set_int(setting, day_conf->events_os[SUNRISE]);
    
    setting = config_setting_add(daytime, "sunset_offset", CONFIG_TYPE_INT);
    config_setting_set_int(setting, day_conf->events_os[SUNSET]);
    
    setting = config_setting_add(daytime, "sunrise", CONFIG_TYPE_STRING);
    config_setting_set_string(setting, day_conf->day_events[SUNRISE]);
    
    setting = config_setting_add(daytime, "sunset", CONFIG_TYPE_STRING);
    config_setting_set_string(setting, day_conf->day_events[SUNSET]);
}

static void store_dimmer_settings(config_t *cfg, dimmer_conf_t *dim_conf) {
    config_setting_t *dimmer = config_setting_add(cfg->root, "dimmer", CONFIG_TYPE_GROUP);
    
    config_setting_t *setting = config_setting_add(dimmer, "disabled", CONFIG_TYPE_BOOL);
    config_setting_set_bool(setting, dim_conf->disabled);
    
    setting = config_setting_add(dimmer, "no_smooth_transition", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_DIM; i++) {
        config_setting_set_bool_elem(setting, -1, dim_conf->smooth[i].no_smooth);
    }
    
    setting = config_setting_add(dimmer, "trans_steps", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_DIM; i++) {
        config_setting_set_float_elem(setting, -1, dim_conf->smooth[i].trans_step);
    }
    
    setting = config_setting_add(dimmer, "trans_timeouts", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_DIM; i++) {
        config_setting_set_int_elem(setting, -1, dim_conf->smooth[i].trans_timeout);
    }
    
    setting = config_setting_add(dimmer, "trans_fixed", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_DIM; i++) {
        config_setting_set_int_elem(setting, -1, dim_conf->smooth[i].trans_fixed);
    }
    
    setting = config_setting_add(dimmer, "dimmed_pct", CONFIG_TYPE_FLOAT);
    config_setting_set_float(setting, dim_conf->dimmed_pct);
    
    setting = config_setting_add(dimmer, "timeouts", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_AC; i++) {
        config_setting_set_int_elem(setting, -1, dim_conf->timeout[i]);
    }
}

static void store_dpms_settings(config_t *cfg, dpms_conf_t *dpms_conf) {
    config_setting_t *dpms = config_setting_add(cfg->root, "dpms", CONFIG_TYPE_GROUP);
    
    config_setting_t *setting = config_setting_add(dpms, "disabled", CONFIG_TYPE_BOOL);
    config_setting_set_bool(setting, dpms_conf->disabled);
    
    setting = config_setting_add(dpms, "timeouts", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_AC; i++) {
        config_setting_set_int_elem(setting, -1, dpms_conf->timeout[i]);
    }
}

static void store_screen_settings(config_t *cfg, screen_conf_t *screen_conf) {
    config_setting_t *screen = config_setting_add(cfg->root, "screen", CONFIG_TYPE_GROUP);
    
    config_setting_t *setting = config_setting_add(screen, "disabled", CONFIG_TYPE_BOOL);
    config_setting_set_bool(setting, screen_conf->disabled);
    
    setting = config_setting_add(screen, "content", CONFIG_TYPE_FLOAT);
    config_setting_set_float(setting, screen_conf->contrib);
    
    setting = config_setting_add(screen, "timeouts", CONFIG_TYPE_ARRAY);
    for (int i = 0; i < SIZE_AC; i++) {
        config_setting_set_int_elem(setting, -1, screen_conf->timeout[i]);
    }
}

static void store_inh_settings(config_t *cfg, inh_conf_t *inh_conf) {
    config_setting_t *inh = config_setting_add(cfg->root, "inhibit", CONFIG_TYPE_GROUP);
    
    config_setting_t *setting = config_setting_add(inh, "disabled", CONFIG_TYPE_BOOL);
    config_setting_set_bool(setting, inh_conf->disabled);
    
    setting = config_setting_add(inh, "inhibit_docked", CONFIG_TYPE_BOOL);
    config_setting_set_bool(setting, inh_conf->inhibit_docked);
    
    setting = config_setting_add(inh, "inhibit_pm", CONFIG_TYPE_BOOL);
    config_setting_set_bool(setting, inh_conf->inhibit_pm);
    
    setting = config_setting_add(inh, "inhibit_bl", CONFIG_TYPE_BOOL);
    config_setting_set_bool(setting, inh_conf->inhibit_bl);
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
    
    config_setting_t *setting = config_setting_add(cfg.root, "verbose", CONFIG_TYPE_BOOL);
    config_setting_set_bool(setting, conf.verbose);
    setting = config_setting_add(cfg.root, "resumedelay", CONFIG_TYPE_INT);
    config_setting_set_int(setting, conf.resumedelay);
    
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
