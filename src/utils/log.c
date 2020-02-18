#include <sys/file.h>
#include <sys/stat.h>
#include "commons.h"

static FILE *log_file;

void open_log(void) {
    char log_path[PATH_MAX + 1] = {0};

    if (getenv("XDG_DATA_HOME")) {
        snprintf(log_path, PATH_MAX, "%s/clight/", getenv("XDG_DATA_HOME"));
    } else {
        snprintf(log_path, PATH_MAX, "%s/.local/share/clight/", getpwuid(getuid())->pw_dir);
    }

    /* Create XDG_DATA_HOME/clight/ folder if it does not exist! */
    mkdir(log_path, 0755);

    strcat(log_path, "clight.log");
    int fd = open(log_path, O_CREAT | O_WRONLY, 0644);
    if (flock(fd, LOCK_EX | LOCK_NB) == -1) {
        WARN("%s\n", strerror(errno));
        ERROR("A lock is present on %s. Another clight instance running?\n", log_path);
    } else {
        log_file = fdopen(fd, "w");
        if (!log_file) {
            WARN("%s\n", strerror(errno));
        } else {
            ftruncate(fd, 0);
        }
    }
}

void log_conf(void) {
    if (log_file) {
        time_t t = time(NULL);

        /* Start with a newline if any log is above */
        fprintf(log_file, "%sClight\n", ftell(log_file) != 0 ? "\n" : "");
        fprintf(log_file, "* Software version:\t\t%s\n", VERSION);
        fprintf(log_file, "* Global config dir:\t\t%s\n", CONFDIR);
        fprintf(log_file, "* Global data dir:\t\t%s\n", DATADIR);
        fprintf(log_file, "* Starting time:\t\t%s\n", ctime(&t));
        
        fprintf(log_file, "Starting options:\n");
        
        fprintf(log_file, "\n### BACKLIGHT ###\n");
        fprintf(log_file, "* Enabled:\t\t%s\n", conf.bl_conf.no_backlight ? "false" : "true");
        fprintf(log_file, "* Smooth trans:\t\t%s\n", conf.bl_conf.no_smooth_backlight ? "Disabled" : "Enabled");
        fprintf(log_file, "* Smooth steps:\t\t%.2lf\n", conf.bl_conf.backlight_trans_step);
        fprintf(log_file, "* Smooth timeout:\t\t%d\n", conf.bl_conf.backlight_trans_timeout);
        fprintf(log_file, "* Daily timeouts:\t\tAC %d\tBATT %d\n", conf.bl_conf.timeout[ON_AC][DAY], conf.bl_conf.timeout[ON_BATTERY][DAY]);
        fprintf(log_file, "* Nightly timeout:\t\tAC %d\tBATT %d\n", conf.bl_conf.timeout[ON_AC][NIGHT], conf.bl_conf.timeout[ON_BATTERY][NIGHT]);
        fprintf(log_file, "* Event timeouts:\t\tAC %d\tBATT %d\n", conf.bl_conf.timeout[ON_AC][SIZE_STATES], conf.bl_conf.timeout[ON_BATTERY][SIZE_STATES]);
        fprintf(log_file, "* Backlight path:\t\t%s\n", strlen(conf.bl_conf.screen_path) ? conf.bl_conf.screen_path : "Unset");
        fprintf(log_file, "* Keyboard backlight:\t\t%s\n", conf.bl_conf.no_keyboard_bl ? "Disabled" : "Enabled");
        fprintf(log_file, "* Shutter threshold:\t\t%.2lf\n", conf.bl_conf.shutter_threshold);
        fprintf(log_file, "* Autocalibration:\t\t%s\n", conf.bl_conf.no_auto_calib ? "Disabled" : "Enabled");
        fprintf(log_file, "* Inhibit on lid closed:\t\t%s\n", conf.bl_conf.inhibit_calib_on_lid_closed ? "Enabled" : "Disabled");
        fprintf(log_file, "* Dim keyboard:\t\t%s\n", conf.bl_conf.dim_kbd ? "Enabled" : "Disabled");
        
        fprintf(log_file, "\n### GAMMA ###\n");
        fprintf(log_file, "* Enabled:\t\t%s\n", conf.gamma_conf.no_gamma ? "false" : "true");
        fprintf(log_file, "* Smooth trans:\t\t%s\n", conf.gamma_conf.no_smooth_gamma ? "Disabled" : "Enabled");
        fprintf(log_file, "* Smooth steps:\t\t%d\n", conf.gamma_conf.gamma_trans_step);
        fprintf(log_file, "* Smooth timeout:\t\t%d\n", conf.gamma_conf.gamma_trans_timeout);
        fprintf(log_file, "* Daily screen temp:\t\t%d\n", conf.gamma_conf.temp[DAY]);
        fprintf(log_file, "* Nightly screen temp:\t\t%d\n", conf.gamma_conf.temp[NIGHT]);
        if (conf.gamma_conf.loc.lat != LAT_UNDEFINED && conf.gamma_conf.loc.lon != LON_UNDEFINED) {
            fprintf(log_file, "* User position:\t\t%.2lf\t%.2lf\n", conf.gamma_conf.loc.lat, conf.gamma_conf.loc.lon);
        } else {
            fprintf(log_file, "* User position:\t\tUnset\n");
        }
        fprintf(log_file, "* User set sunrise:\t\t%s\n", strlen(conf.gamma_conf.day_events[SUNRISE]) ? conf.gamma_conf.day_events[SUNRISE] : "Unset");
        fprintf(log_file, "* User set sunset:\t\t%s\n", strlen(conf.gamma_conf.day_events[SUNSET]) ? conf.gamma_conf.day_events[SUNSET] : "Unset");
        fprintf(log_file, "* Event duration:\t\t%d\n", conf.gamma_conf.event_duration);
        fprintf(log_file, "* Long transition:\t\t%s\n", conf.gamma_conf.gamma_long_transition ? "Enabled" : "Disabled");
        fprintf(log_file, "* Ambient gamma:\t\t%s\n", conf.gamma_conf.ambient_gamma ? "Enabled" : "Disabled");
        
        fprintf(log_file, "\n### DIMMER ###\n");
        fprintf(log_file, "* Enabled:\t\t%s\n", conf.dim_conf.no_dimmer ? "false" : "true");
        fprintf(log_file, "* Smooth trans:\t\tENTER: %s, EXIT: %s\n", 
                conf.dim_conf.no_smooth_dimmer[ENTER] ? "Disabled" : "Enabled",
                conf.dim_conf.no_smooth_dimmer[EXIT] ? "Disabled" : "Enabled");
        fprintf(log_file, "* Smooth steps:\t\tENTER: %.2lf, EXIT: %.2lf\n", conf.dim_conf.dimmer_trans_step[ENTER], conf.dim_conf.dimmer_trans_step[EXIT]);
        fprintf(log_file, "* Smooth timeout:\t\tENTER: %d, EXIT: %d\n", conf.dim_conf.dimmer_trans_timeout[ENTER], conf.dim_conf.dimmer_trans_timeout[EXIT]);
        fprintf(log_file, "* Timeouts:\t\tAC %d\tBATT %d\n", conf.dim_conf.dimmer_timeout[ON_AC], conf.dim_conf.dimmer_timeout[ON_BATTERY]);
        fprintf(log_file, "* Backlight pct:\t\t%.2lf\n", conf.dim_conf.dimmer_pct);
        
        fprintf(log_file, "\n### DPMS ###\n");
        fprintf(log_file, "* Enabled:\t\t%s\n", conf.dpms_conf.no_dpms ? "false" : "true");
        fprintf(log_file, "* Timeouts:\t\tAC %d\tBATT %d\n", conf.dpms_conf.dpms_timeout[ON_AC], conf.dpms_conf.dpms_timeout[ON_BATTERY]);
        
        fprintf(log_file, "\n### SCREEN ###\n");
        fprintf(log_file, "* Enabled:\t\t%s\n", conf.screen_conf.no_screen ? "false" : "true");
        fprintf(log_file, "* Timeouts:\t\tAC %d\tBATT %d\n", conf.screen_conf.screen_timeout[ON_AC], conf.screen_conf.screen_timeout[ON_BATTERY]);
        fprintf(log_file, "* Contrib:\t\t%.2lf\n", conf.screen_conf.screen_contrib);
        fprintf(log_file, "* Samples:\t\t%d\n", conf.screen_conf.screen_samples);
        
        fprintf(log_file, "\n### SENSOR ###\n");
        fprintf(log_file, "* Captures:\t\tAC %d\tBATT %d\n", conf.sens_conf.num_captures[ON_AC], conf.sens_conf.num_captures[ON_BATTERY]);
        fprintf(log_file, "* Device:\t\t%s\n", strlen(conf.sens_conf.dev_name) ? conf.sens_conf.dev_name : "Unset");
        fprintf(log_file, "* Settings:\t\t%s\n", strlen(conf.sens_conf.dev_opts) ? conf.sens_conf.dev_opts : "Unset");

        fprintf(log_file, "\n### GENERIC ###\n");
        fprintf(log_file, "* Verbose (debug):\t\t%s\n", conf.verbose ? "Enabled" : "Disabled");
        fprintf(log_file, "* Inhibit docked:\t\t%s\n\n", conf.inhibit_docked ? "Enabled" : "Disabled");
        
        fflush(log_file);
    }
}

void log_message(const char *filename, int lineno, const char type, const char *log_msg, ...) {
    va_list file_args, args;

    va_start(file_args, log_msg);
    va_copy(args, file_args);
    if (log_file) {
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        fprintf(log_file, "(%c)[%02d:%02d:%02d]{%s:%d}\t", type, tm->tm_hour, tm->tm_min, tm->tm_sec, filename, lineno);
        vfprintf(log_file, log_msg, file_args);
        fflush(log_file);
    }

    /* In case of error, log to stdout too */
    FILE *out = stdout;
    if (type == 'E') {
        out = stderr;
    }

    vfprintf(out, log_msg, args);
    va_end(args);
    va_end(file_args);
}

void close_log(void) {
    if (log_file) {
        flock(fileno(log_file), LOCK_UN);
        fclose(log_file);
    }
}
