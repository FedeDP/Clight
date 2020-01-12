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
        fprintf(log_file, "\n### Generic ###\n");
        fprintf(log_file, "* Verbose (debugging):\t\t%s\n", conf.verbose ? "Enabled" : "Disabled");
        fprintf(log_file, "* Autocalibration:\t\t%s\n", conf.no_auto_calib ? "Disabled" : "Enabled");
        fprintf(log_file, "* Inhibit autocalibration:\t\t%s\n", conf.inhibit_autocalib ? "Enabled" : "Disabled");
        fprintf(log_file, "* Keyboard backlight:\t\t%s\n", conf.no_keyboard_bl ? "Disabled" : "Enabled");
        fprintf(log_file, "* Number of captures:\t\t%d\n", conf.num_captures);
        fprintf(log_file, "* Sensor device:\t\t%s\n", strlen(conf.dev_name) ? conf.dev_name : "Unset");
        fprintf(log_file, "* Backlight path:\t\t%s\n", strlen(conf.screen_path) ? conf.screen_path : "Unset");
        fprintf(log_file, "* Shutter threshold:\t\t%.2lf\n", conf.shutter_threshold);
        fprintf(log_file, "* Ambient gamma:\t\t%s\n", conf.ambient_gamma ? "Enabled" : "Disabled");
        fprintf(log_file, "* Screen contrib:\t\t%.2lf\n", conf.screen_contrib);
        fprintf(log_file, "* Screen samples:\t\t%d\n", conf.screen_samples);

        if (conf.loc.lat != LAT_UNDEFINED && conf.loc.lon != LON_UNDEFINED) {
            fprintf(log_file, "* User position:\t\t%.2lf\t%.2lf\n", conf.loc.lat, conf.loc.lon);
        } else {
            fprintf(log_file, "* User position:\t\tUnset\n");
        }
        fprintf(log_file, "* Daily screen temp:\t\t%d\n", conf.temp[DAY]);
        fprintf(log_file, "* Nightly screen temp:\t\t%d\n", conf.temp[NIGHT]);
        fprintf(log_file, "* User set sunrise:\t\t%s\n", strlen(conf.day_events[SUNRISE]) ? conf.day_events[SUNRISE] : "Unset");
        fprintf(log_file, "* User set sunset:\t\t%s\n", strlen(conf.day_events[SUNSET]) ? conf.day_events[SUNSET] : "Unset");
        fprintf(log_file, "* Event duration:\t\t%d\n", conf.event_duration);
        fprintf(log_file, "* Dimmer backlight pct:\t\t%.2lf\n", conf.dimmer_pct);

        fprintf(log_file, "\n### Timeouts ###\n");
        fprintf(log_file, "* Daily timeouts:\t\tAC %d\tBATT %d\n", conf.timeout[ON_AC][DAY], conf.timeout[ON_BATTERY][DAY]);
        fprintf(log_file, "* Nightly timeout:\t\tAC %d\tBATT %d\n", conf.timeout[ON_AC][NIGHT], conf.timeout[ON_BATTERY][NIGHT]);
        fprintf(log_file, "* Event timeouts:\t\tAC %d\tBATT %d\n", conf.timeout[ON_AC][SIZE_STATES], conf.timeout[ON_BATTERY][SIZE_STATES]);
        fprintf(log_file, "* Dimmer timeouts:\t\tAC %d\tBATT %d\n", conf.dimmer_timeout[ON_AC], conf.dimmer_timeout[ON_BATTERY]);
        fprintf(log_file, "* Dpms timeouts:\t\tAC %d\tBATT %d\n", conf.dpms_timeout[ON_AC], conf.dpms_timeout[ON_BATTERY]);
        fprintf(log_file, "* Screen timeouts:\t\tAC %d\tBATT %d\n", conf.screen_timeout[ON_AC], conf.screen_timeout[ON_BATTERY]);

        fprintf(log_file, "\n### Modules ###\n");
        fprintf(log_file, "* Backlight:\t\t%s\n", conf.no_backlight ? "Disabled" : "Enabled");
        fprintf(log_file, "* Gamma:\t\t%s\n", conf.no_gamma ? "Disabled" : "Enabled");
        fprintf(log_file, "* Dimmer:\t\t%s\n", conf.no_dimmer ? "Disabled" : "Enabled");
        fprintf(log_file, "* Dpms:\t\t%s\n", conf.no_dpms ? "Disabled" : "Enabled");
        fprintf(log_file, "* Screen:\t\t%s\n", conf.no_screen ? "Disabled" : "Enabled");

        fprintf(log_file, "\n### Smooth ###\n");
        fprintf(log_file, "* Bright smooth trans:\t\t%s\n", conf.no_smooth_backlight ? "Disabled" : "Enabled");
        fprintf(log_file, "* Gamma smooth trans:\t\t%s\n", conf.no_smooth_gamma ? "Disabled" : "Enabled");
        fprintf(log_file, "* Dimmer smooth trans:\t\tENTER: %s, EXIT: %s\n", 
                conf.no_smooth_dimmer[ENTER] ? "Disabled" : "Enabled",
                conf.no_smooth_dimmer[EXIT] ? "Disabled" : "Enabled");
        fprintf(log_file, "* Bright smooth steps:\t\t%.2lf\n", conf.backlight_trans_step);
        fprintf(log_file, "* Gamma smooth steps:\t\t%d\n", conf.gamma_trans_step);
        fprintf(log_file, "* Dimmer smooth steps:\t\tENTER: %.2lf, EXIT: %.2lf\n", conf.dimmer_trans_step[ENTER], conf.dimmer_trans_step[EXIT]);
        fprintf(log_file, "* Bright smooth timeout:\t\t%d\n", conf.backlight_trans_timeout);
        fprintf(log_file, "* Gamma smooth timeout:\t\t%d\n", conf.gamma_trans_timeout);
        fprintf(log_file, "* Dimmer smooth timeout:\t\tENTER: %d, EXIT: %d\n", conf.dimmer_trans_timeout[ENTER], conf.dimmer_trans_timeout[EXIT]);
        fprintf(log_file, "* Gamma long transition:\t\t%s\n\n", conf.gamma_long_transition ? "Enabled" : "Disabled");

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
