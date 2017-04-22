#include "../inc/config.h"
#include <libconfig.h>

static void init_config_file(enum CONFIG file);

static char config_file[PATH_MAX + 1];

/**
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

/* 
 * Ask user to perform some operations on webcam to understand max and minimum brightness levels
 * and to better fit screen backlight level to ambient brightness.
 */
// void setup_config(void) {
//     config_t cfg;
//     config_setting_t *root, *setting;
//     int x = 0;
//     char syspath[PATH_MAX + 1] = {0};
//     char devpath[PATH_MAX + 1] = {0};
//     
//     config_init(&cfg);
//     root = config_root_setting(&cfg);
//     
//     // delete old config file if present
//     if (access(config_file, F_OK ) == 0) {
//         fprintf(stderr, "Config file already present. Removing old one.\n");
//         remove(config_file);
//     }
//     
//     do {
//         printf("Enter number of frames for each capture:> ");
//         scanf("%d", &x);
//     } while (x <= 0);
//     setting = config_setting_add(root, "frames", CONFIG_TYPE_INT);
//     config_setting_set_int(setting, x);
//     
//     do {
//         printf("Enter timeout between captures in seconds:> ");
//         scanf("%d", &x);
//     } while (x <= 0);
//     setting = config_setting_add(root, "timeout", CONFIG_TYPE_INT);
//     config_setting_set_int(setting, x);
//     
//     do {
//         printf("Enter daily gamma temperature:> ");
//         scanf("%d", &x);
//     } while (x <= 0);
//     setting = config_setting_add(root, "day_temp", CONFIG_TYPE_INT);
//     config_setting_set_int(setting, x);
//     
//     do {
//         printf("Enter nightly gamma temperature:> ");
//         scanf("%d", &x);
//     } while (x <= 0);
//     setting = config_setting_add(root, "night_temp", CONFIG_TYPE_INT);
//     config_setting_set_int(setting, x);
//     
//     do {
//         printf("Enable smooth gamma transitions? (1/0):> ");
//         scanf("%d", &x);
//     } while (x != 0 && x != 1);
//     setting = config_setting_add(root, "smooth_gamma_transition", CONFIG_TYPE_INT);
//     config_setting_set_int(setting, x);
//     
//     /**
//      * Video and screen sysnames can be empty
//      */
//     printf("Enter webcam device sysname (eg: video0, look in /dev/video*).\nIf left blank, first found device will be used. :> ");
//     scanf("%s", devpath);
//     setting = config_setting_add(root, "video_sysname", CONFIG_TYPE_STRING);
//     config_setting_set_string(setting, devpath);
//     
//     printf("Enter default backlight kernel interface (eg: intel_backlight, look in /sys/class/backlight/*).\nIf left blank, first found device will be used. :> ");
//     scanf("%s", syspath);
//     
//     setting = config_setting_add(root, "screen_sysname", CONFIG_TYPE_STRING);
//     config_setting_set_string(setting, syspath);
//     
//     /* Write out the new configuration. */
//     if(!config_write_file(&cfg, config_file)) {
//         fprintf(stderr, "Error while writing file.\n");
//     } else {
//         printf("New configuration successfully written to: %s\n", config_file);
//     }
//     
//     config_destroy(&cfg);
// }

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
        config_lookup_int(&cfg, "day_timeout", &conf.timeout[DAY]);
        config_lookup_int(&cfg, "night_timeout", &conf.timeout[NIGHT]);
        config_lookup_int(&cfg, "event_timeout", &conf.timeout[EVENT]);
        config_lookup_int(&cfg, "day_temp", &conf.temp[DAY]);
        config_lookup_int(&cfg, "night_temp", &conf.temp[NIGHT]);
        config_lookup_int(&cfg, "no_smooth_gamma_transition", &conf.no_smooth_transition);
        config_lookup_int(&cfg, "no_gamma", &modules[GAMMA_IX].disabled);
        config_lookup_float(&cfg, "latitude", &conf.lat);
        config_lookup_float(&cfg, "longitude", &conf.lon);
        
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

    } else {
        WARN("Config file: %s at line %d.\n",
                config_error_text(&cfg),
                config_error_line(&cfg));
    }
    config_destroy(&cfg);
}
