#include "../inc/log.h"

static FILE *log_file;

void open_log(void) {
    const char log_name[] = "clight.log";

    char log_path[PATH_MAX + 1] = {0};

    snprintf(log_path, PATH_MAX, "%s/.%s", getpwuid(getuid())->pw_dir, log_name);
    log_file = fopen(log_path, "w");
    if (!log_file) {
        WARN("%s\n", strerror(errno));
    }
}

void log_conf(void) {
    if (log_file) {
        time_t t = time(NULL);

        fprintf(log_file, "Clight\n");
        fprintf(log_file, "Version: %s\n", VERSION);
        fprintf(log_file, "Time: %s", ctime(&t));
        fprintf(log_file, "\nStarting options:\n");
        fprintf(log_file, "* Number of captures:\t\t%d\n", conf.num_captures);
        fprintf(log_file, "* Daily timeouts:\t\tAC %d\tBATT %d\n", conf.timeout[ON_AC][DAY], conf.timeout[ON_BATTERY][DAY]);
        fprintf(log_file, "* Nightly timeout:\t\tAC %d\tBATT %d\n", conf.timeout[ON_AC][NIGHT], conf.timeout[ON_BATTERY][NIGHT]);
        fprintf(log_file, "* Event timeouts:\t\tAC %d\tBATT %d\n", conf.timeout[ON_AC][EVENT], conf.timeout[ON_BATTERY][EVENT]);
        fprintf(log_file, "* Webcam device:\t\t%s\n", conf.dev_name);
        fprintf(log_file, "* Backlight path:\t\t%s\n", conf.screen_path);
        fprintf(log_file, "* Daily screen temp:\t\t%d\n", conf.temp[DAY]);
        fprintf(log_file, "* Nightly screen temp:\t\t%d\n", conf.temp[NIGHT]);
        fprintf(log_file, "* Smooth transitions:\t\t%s\n", conf.no_smooth_transition ? "disabled" : "enabled");
        fprintf(log_file, "* User latitude:\t\t%.2lf\n", conf.lat);
        fprintf(log_file, "* User longitude:\t\t%.2lf\n", conf.lon);
        fprintf(log_file, "* User setted sunrise:\t\t%s\n", conf.events[SUNRISE]);
        fprintf(log_file, "* User setted sunset:\t\t%s\n", conf.events[SUNSET]);
        fprintf(log_file, "* Gamma correction:\t\t%s\n", conf.no_gamma ? "disabled" : "enabled");
        fprintf(log_file, "* Min backlight level:\t\t%d\n", conf.lowest_backlight_level);
        fprintf(log_file, "* Event duration:\t\t%d\n", conf.event_duration);
        fprintf(log_file, "* Screen dimmer tool:\t\t%s\n", conf.no_dimmer ? "disabled" : "enabled");
        fprintf(log_file, "* Dimmer backlight:\t\t%d%%\n", conf.dimmer_pct);
        fprintf(log_file, "* Dimmer timeouts:\t\tAC %d\tBATT %d\n", conf.dimmer_timeout[ON_AC], conf.dimmer_timeout[ON_BATTERY]);
        fprintf(log_file, "* Screen dpms tool:\t\t%s\n", conf.no_dpms ? "disabled" : "enabled");
        fprintf(log_file, "* Dpms timeouts:\t\tAC %d:%d:%d\tBATT %d:%d:%d\n\n",
                conf.dpms_timeouts[ON_AC][STANDBY], conf.dpms_timeouts[ON_AC][SUSPEND], conf.dpms_timeouts[ON_AC][OFF],
                conf.dpms_timeouts[ON_BATTERY][STANDBY], conf.dpms_timeouts[ON_BATTERY][SUSPEND], conf.dpms_timeouts[ON_BATTERY][OFF]
        );
    }
}

void log_message(const char *filename, int lineno, const char type, const char *log_msg, ...) {
    va_list file_args, args;

    va_start(file_args, log_msg);
    va_copy(args, file_args);
    if (log_file) {
        fprintf(log_file, "(%c) [%s:%d]\t", type, filename, lineno);
        vfprintf(log_file, log_msg, file_args);
        fflush(log_file);
    }
    
    /* In case of error, set quit flag */
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
        fclose(log_file);
    }
}
