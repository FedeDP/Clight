#include "../inc/config.h"

static char config_file[PATH_MAX + 1];

/**
 * If an user is specified with the cmdline option,
 * use that to retrieve the config file.
 * Fallback to getpwuid
 */
void init_config_file(void) {
    snprintf(config_file, PATH_MAX, "%s/.config/clight.conf", getpwuid(getuid())->pw_dir);
}

void setup_config(void) {
    config_t cfg;
    config_setting_t *root, *setting;
    int num_frames = 0, t = 0;
    char syspath[PATH_MAX + 1] = {0};
    char devpath[PATH_MAX + 1] = {0};
    
    config_init(&cfg);
    root = config_root_setting(&cfg);
    
    // delete old config file if present
    if (access(config_file, F_OK ) == 0) {
        _log(stderr, "Config file already present. Removing old one.\n\n");
        remove(config_file);
    }
    
    do {
        printf("Enter number of frames for each capture:> ");
        scanf("%d", &num_frames);
    } while (num_frames <= 0);
    
    setting = config_setting_add(root, "frames", CONFIG_TYPE_INT);
    config_setting_set_int(setting, num_frames);
    
    do {
        printf("Enter timeout between captures in seconds:> ");
        scanf("%d", &t);
    } while (t <= 0);
    
    setting = config_setting_add(root, "timeout", CONFIG_TYPE_INT);
    config_setting_set_int(setting, t);
    
    do {
        printf("Enter webcam device full devpath (/dev/videoX):> ");
        scanf("%s", devpath);
    } while (strlen(devpath) == 0);
    
    setting = config_setting_add(root, "video_devpath", CONFIG_TYPE_STRING);
    config_setting_set_string(setting, devpath);
    
    do {
        printf("Enter default backlight full syspath (look in /sys/class/backlight/*):> ");
        scanf("%s", syspath);
    } while (strlen(syspath) == 0);
    
    setting = config_setting_add(root, "screen_syspath", CONFIG_TYPE_STRING);
    config_setting_set_string(setting, syspath);
    
    /* Write out the new configuration. */
    if(!config_write_file(&cfg, config_file)) {
        _log(stderr, "Error while writing file.\n");
    } else {
        _log(stdout, "New configuration successfully written to: %s\n", config_file);
    }
        
    config_destroy(&cfg);
}

void read_config(void) {
    config_t cfg;
    const char *videodev, *screendev;
    
    if (access(config_file, F_OK) == -1) {
        _log(stderr, "Config file %s not found.\n", config_file);
        return;
    }
    
    config_init(&cfg);
    if (config_read_file(&cfg, config_file) == CONFIG_TRUE) {
        config_lookup_int(&cfg, "frames", &conf.num_captures);
        config_lookup_int(&cfg, "timeout", &conf.timeout);
        
        if (config_lookup_string(&cfg, "video_devpath", &videodev) == CONFIG_TRUE) {
            strncpy(conf.dev_name, videodev, PATH_MAX);
        }
        
        if (config_lookup_string(&cfg, "screen_syspath", &screendev) == CONFIG_TRUE) {
            strncpy(conf.screen_path, screendev, PATH_MAX);
        }
        
    } else {
        _log(stderr, "Config file: %s at line %d.\n",
                config_error_text(&cfg),
                config_error_line(&cfg));
    }
    config_destroy(&cfg);
}
