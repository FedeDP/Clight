#include "../inc/log.h"
#include "../inc/modules.h"

static FILE *log_file;

void open_log(void) {
    char log_path[PATH_MAX + 1] = {0};

    snprintf(log_path, PATH_MAX, "%s/.%s", getpwuid(getuid())->pw_dir, 
             conf.single_capture_mode ? "clight_capture.log" : "clight.log");
    log_file = fopen(log_path, "w");
    if (!log_file) {
        WARN("%s\n", strerror(errno));
    }
}

void log_conf(void) {
    if (log_file) {
        fprintf(log_file, "Clight\n");
        fprintf(log_file, "* Software version:\t\t%s\n", VERSION);
        fprintf(log_file, "* Starting time:\t\t%s %s\n", __DATE__ , __TIME__);
        fprintf(log_file, "Starting options:\n");
        fprintf(log_file, "\n### Generic ###\n");
        fprintf(log_file, "* Verbose (debugging):\t\t%s\n", conf.verbose ? "Enabled" : "Disabled");
        fprintf(log_file, "* Number of captures:\t\t%d\n", conf.num_captures);
        fprintf(log_file, "* Webcam device:\t\t%s\n", strlen(conf.dev_name) ? conf.dev_name : "Unsetted");
        fprintf(log_file, "* Backlight path:\t\t%s%s", strlen(conf.screen_path) ? conf.screen_path : "Unsetted", conf.single_capture_mode ? "\n\n" : "\n");
        
        if (!conf.single_capture_mode) {
            if (conf.loc.lat != 0.0f || conf.loc.lon != 0.0f) {
                fprintf(log_file, "* User position:\t\t%.2lf\t%.2lf\n", conf.loc.lat, conf.loc.lon);
            } else {
                fprintf(log_file, "* User position:\t\tUnsetted\n");
            }
            fprintf(log_file, "* Daily screen temp:\t\t%d\n", conf.temp[DAY]);
            fprintf(log_file, "* Nightly screen temp:\t\t%d\n", conf.temp[NIGHT]);
            fprintf(log_file, "* User setted sunrise:\t\t%s\n", strlen(conf.events[SUNRISE]) ? conf.events[SUNRISE] : "Unsetted");
            fprintf(log_file, "* User setted sunset:\t\t%s\n", strlen(conf.events[SUNSET]) ? conf.events[SUNSET] : "Unsetted");
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
            fprintf(log_file, "* Brightness correction:\t\t%s\n", is_started_disabled(BRIGHTNESS) ? "Disabled" : "Enabled");
            fprintf(log_file, "* Gamma correction:\t\t%s\n", is_started_disabled(GAMMA) ? "Disabled" : "Enabled");
            fprintf(log_file, "* Screen dpms tool:\t\t%s\n", is_started_disabled(DPMS) ? "Disabled" : "Enabled");
            fprintf(log_file, "* Screen dimmer tool:\t\t%s\n", is_started_disabled(DIMMER) ? "Disabled" : "Enabled");
            
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
            
        }
        fflush(log_file);
    }
}

void log_message(const char *filename, int lineno, const char type, const char *log_msg, ...) {
    va_list file_args, args;

    va_start(file_args, log_msg);
    va_copy(args, file_args);
    if (log_file) {
        fprintf(log_file, "(%c)[%s]{%s:%d}\t", type, __TIME__, filename, lineno);
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
        fclose(log_file);
    }
}
