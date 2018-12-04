#include <log.h>
#include <modules.h>
#include <sys/file.h>

static FILE *log_file;

void open_log(void) {
    char log_path[PATH_MAX + 1] = {0};

    snprintf(log_path, PATH_MAX, "%s/.clight.log", getpwuid(getuid())->pw_dir);
    log_file = fopen(log_path, "w");
    if (!log_file) {
        WARN("%s\n", strerror(errno));
    }

    if (flock(fileno(log_file), LOCK_EX | LOCK_NB) == -1) {
        WARN("%s\n", strerror(errno));
        ERROR("A lock is present on %s. Another clight instance running?\n", log_path);
    }
}

void log_conf(void) {
    if (log_file) {
        time_t t = time(NULL);

        fprintf(log_file, "Clight\n");
        fprintf(log_file, "* Software version:\t\t%s\n", VERSION);
        fprintf(log_file, "* Starting time:\t\t%s\n", ctime(&t));
        fprintf(log_file, "Starting options:\n");
        fprintf(log_file, "\n### Generic ###\n");
        fprintf(log_file, "* Verbose (debugging):\t\t%s\n", conf.verbose ? "Enabled" : "Disabled");
        fprintf(log_file, "* Automatic calibration:\t\t%s\n", conf.no_auto_calib ? "Disabled" : "Enabled");
        fprintf(log_file, "* Keyboard backlight:\t\t%s\n", conf.no_keyboard_bl ? "Disabled" : "Enabled");
        fprintf(log_file, "* Number of captures:\t\t%d\n", conf.num_captures);
        fprintf(log_file, "* Webcam device:\t\t%s\n", strlen(conf.dev_name) ? conf.dev_name : "Unset");
        fprintf(log_file, "* Backlight path:\t\t%s\n", strlen(conf.screen_path) ? conf.screen_path : "Unset");
        fprintf(log_file, "* Shutter threshold:\t\t%.2lf\n", conf.shutter_threshold);

        if (conf.loc.lat != 0.0f || conf.loc.lon != 0.0f) {
            fprintf(log_file, "* User position:\t\t%.2lf\t%.2lf\n", conf.loc.lat, conf.loc.lon);
        } else {
            fprintf(log_file, "* User position:\t\tUnset\n");
        }
        fprintf(log_file, "* Daily screen temp:\t\t%d\n", conf.temp[DAY]);
        fprintf(log_file, "* Nightly screen temp:\t\t%d\n", conf.temp[NIGHT]);
        fprintf(log_file, "* User set sunrise:\t\t%s\n", strlen(conf.events[SUNRISE]) ? conf.events[SUNRISE] : "Unset");
        fprintf(log_file, "* User set sunset:\t\t%s\n", strlen(conf.events[SUNSET]) ? conf.events[SUNSET] : "Unset");
        fprintf(log_file, "* Event duration:\t\t%d\n", conf.event_duration);
        fprintf(log_file, "* Dimmer backlight pct:\t\t%.2lf\n", conf.dimmer_pct);

        fprintf(log_file, "\n### Timeouts ###\n");
        fprintf(log_file, "* Daily timeouts:\t\tAC %d\tBATT %d\n", conf.timeout[ON_AC][DAY], conf.timeout[ON_BATTERY][DAY]);
        fprintf(log_file, "* Nightly timeout:\t\tAC %d\tBATT %d\n", conf.timeout[ON_AC][NIGHT], conf.timeout[ON_BATTERY][NIGHT]);
        fprintf(log_file, "* Event timeouts:\t\tAC %d\tBATT %d\n", conf.timeout[ON_AC][EVENT], conf.timeout[ON_BATTERY][EVENT]);
        fprintf(log_file, "* Dimmer timeouts:\t\tAC %d\tBATT %d\n", conf.dimmer_timeout[ON_AC], conf.dimmer_timeout[ON_BATTERY]);
        fprintf(log_file, "* Dpms timeouts:\t\tAC %d:%d:%d\tBATT %d:%d:%d\n",
                conf.dpms_timeouts[ON_AC][STANDBY], conf.dpms_timeouts[ON_AC][SUSPEND], conf.dpms_timeouts[ON_AC][OFF],
                conf.dpms_timeouts[ON_BATTERY][STANDBY], conf.dpms_timeouts[ON_BATTERY][SUSPEND], conf.dpms_timeouts[ON_BATTERY][OFF]
        );

        fprintf(log_file, "\n### Modules ###\n");
        for (int i = 0; i < MODULES_NUM; i++) {
            const struct self_t *self = modules[i].self;
            if (self->functional_module) {
                fprintf(log_file, "* %s:\t\t%s\n", self->name, is_started_disabled(i) ? "Disabled" : "Enabled");
            }
        }

        fprintf(log_file, "\n### Smooth ###\n");
        fprintf(log_file, "* Bright smooth trans:\t\t%s\n", conf.no_smooth_backlight ? "Disabled" : "Enabled");
        fprintf(log_file, "* Gamma smooth trans:\t\t%s\n", conf.no_smooth_gamma ? "Disabled" : "Enabled");
        fprintf(log_file, "* Dimmer smooth trans:\t\t%s\n", conf.no_smooth_dimmer ? "Disabled" : "Enabled");
        fprintf(log_file, "* Bright smooth steps:\t\t%.2lf\n", conf.backlight_trans_step);
        fprintf(log_file, "* Gamma smooth steps:\t\t%d\n", conf.gamma_trans_step);
        fprintf(log_file, "* Dimmer smooth steps:\t\t%.2lf\n", conf.dimmer_trans_step);
        fprintf(log_file, "* Bright smooth timeout:\t\t%d\n", conf.backlight_trans_timeout);
        fprintf(log_file, "* Gamma smooth timeout:\t\t%d\n", conf.gamma_trans_timeout);
        fprintf(log_file, "* Dimmer smooth timeout:\t\t%d\n\n", conf.dimmer_trans_timeout);

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
