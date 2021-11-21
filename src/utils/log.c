#include <sys/file.h>
#include <sys/stat.h>
#include "commons.h"

static void log_bl_smooth(bl_smooth_t *smooth, const char *prefix);
static void log_bl_conf(bl_conf_t *bl_conf);
static void log_sens_conf(sensor_conf_t *sens_conf);
static void log_kbd_conf(kbd_conf_t *kbd_conf);
static void log_gamma_conf(gamma_conf_t *gamma_conf);
static void log_daytime_conf(daytime_conf_t *day_conf);
static void log_dim_conf(dimmer_conf_t *dim_conf);
static void log_dpms_conf(dpms_conf_t *dpms_conf);
static void log_scr_conf(screen_conf_t *screen_conf);
static void log_inh_conf(inh_conf_t *inh_conf);

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
    }
    
    log_file = fdopen(fd, "w");
    if (!log_file) {
        ERROR("%s\n", strerror(errno));
    } 
    
    ftruncate(fd, 0);
}

static void log_bl_smooth(bl_smooth_t *smooth, const char *prefix) {
    fprintf(log_file, "* %sSmooth trans:\t\t%s\n", prefix, smooth->no_smooth ? "Disabled" : "Enabled");
    fprintf(log_file, "* %sSmooth step:\t\t%.2lf\n", prefix, smooth->trans_step);
    fprintf(log_file, "* %sSmooth timeout:\t\t%d\n", prefix, smooth->trans_timeout);
    fprintf(log_file, "* %sSmooth fixed:\t\t%d\n", prefix, smooth->trans_fixed);
}

static void log_bl_conf(bl_conf_t *bl_conf) {
    fprintf(log_file, "\n### BACKLIGHT ###\n");
    log_bl_smooth(&conf.bl_conf.smooth, "");
    fprintf(log_file, "* Daily timeouts:\t\tAC %d\tBATT %d\n", bl_conf->timeout[ON_AC][DAY], bl_conf->timeout[ON_BATTERY][DAY]);
    fprintf(log_file, "* Nightly timeouts:\t\tAC %d\tBATT %d\n", bl_conf->timeout[ON_AC][NIGHT], bl_conf->timeout[ON_BATTERY][NIGHT]);
    fprintf(log_file, "* Event timeouts:\t\tAC %d\tBATT %d\n", bl_conf->timeout[ON_AC][SIZE_STATES], bl_conf->timeout[ON_BATTERY][SIZE_STATES]);
    fprintf(log_file, "* Shutter threshold:\t\t%.2lf\n", bl_conf->shutter_threshold);
    fprintf(log_file, "* Autocalibration:\t\t%s\n", bl_conf->no_auto_calib ? "Disabled" : "Enabled");
    fprintf(log_file, "* Pause on lid closed:\t\t%s\n", bl_conf->pause_on_lid_closed ? "Enabled" : "Disabled");
    fprintf(log_file, "* Capture on lid opened:\t\t%s\n", bl_conf->capture_on_lid_opened ? "Enabled" : "Disabled");
    fprintf(log_file, "* Restore On Exit:\t\t%s\n", bl_conf->restore ? "Enabled" : "Disabled");
}

static void log_sens_conf(sensor_conf_t *sens_conf) {
    fprintf(log_file, "\n### SENSOR ###\n");
    fprintf(log_file, "* Captures:\t\tAC %d\tBATT %d\n", sens_conf->num_captures[ON_AC], sens_conf->num_captures[ON_BATTERY]);
    fprintf(log_file, "* Device:\t\t%s\n", sens_conf->dev_name ? sens_conf->dev_name : "Unset");
    fprintf(log_file, "* Settings:\t\t%s\n", sens_conf->dev_opts ? sens_conf->dev_opts : "Unset");
}

static void log_kbd_conf(kbd_conf_t *kbd_conf) {
    fprintf(log_file, "\n### KEYBOARD ###\n");
    fprintf(log_file, "* Dim:\t\t%s\n", kbd_conf->dim ? "Enabled" : "Disabled");
    fprintf(log_file, "* Timeouts:\t\tAC %d\tBATT %d\n", kbd_conf->timeout[ON_AC], kbd_conf->timeout[ON_BATTERY]);
}

static void log_gamma_conf(gamma_conf_t *gamma_conf) {
    fprintf(log_file, "\n### GAMMA ###\n");
    fprintf(log_file, "* Smooth trans:\t\t%s\n", gamma_conf->no_smooth ? "Disabled" : "Enabled");
    fprintf(log_file, "* Smooth steps:\t\t%d\n", gamma_conf->trans_step);
    fprintf(log_file, "* Smooth timeout:\t\t%d\n", gamma_conf->trans_timeout);
    fprintf(log_file, "* Daily screen temp:\t\t%d\n", gamma_conf->temp[DAY]);
    fprintf(log_file, "* Nightly screen temp:\t\t%d\n", gamma_conf->temp[NIGHT]);
    fprintf(log_file, "* Long transition:\t\t%s\n", gamma_conf->long_transition ? "Enabled" : "Disabled");
    fprintf(log_file, "* Ambient gamma:\t\t%s\n", gamma_conf->ambient_gamma ? "Enabled" : "Disabled");
    fprintf(log_file, "* Restore On Exit:\t\t%s\n", gamma_conf->restore ? "Enabled" : "Disabled");
}

static void log_daytime_conf(daytime_conf_t *day_conf) {
    fprintf(log_file, "\n### DAYTIME ###\n");
    if (day_conf->loc.lat != LAT_UNDEFINED && day_conf->loc.lon != LON_UNDEFINED) {
        fprintf(log_file, "* User position:\t\t%.2lf\t%.2lf\n", day_conf->loc.lat, day_conf->loc.lon);
    } else {
        fprintf(log_file, "* User position:\t\tUnset\n");
    }
    fprintf(log_file, "* User set sunrise:\t\t%s\n", strlen(day_conf->day_events[SUNRISE]) ? day_conf->day_events[SUNRISE] : "Unset");
    fprintf(log_file, "* User set sunset:\t\t%s\n", strlen(day_conf->day_events[SUNSET]) ? day_conf->day_events[SUNSET] : "Unset");
    fprintf(log_file, "* Event duration:\t\t%d\n", day_conf->event_duration);
    fprintf(log_file, "* Sunrise offset:\t\t%d\n", day_conf->events_os[SUNRISE]);
    fprintf(log_file, "* Sunset offset:\t\t%d\n", day_conf->events_os[SUNSET]);
}

static void log_dim_conf(dimmer_conf_t *dim_conf) {
    fprintf(log_file, "\n### DIMMER ###\n");
    log_bl_smooth(&conf.dim_conf.smooth[ENTER], "ENTER ");
    log_bl_smooth(&conf.dim_conf.smooth[EXIT], "EXIT ");
    fprintf(log_file, "* Timeouts:\t\tAC %d\tBATT %d\n", dim_conf->timeout[ON_AC], dim_conf->timeout[ON_BATTERY]);
    fprintf(log_file, "* Backlight pct:\t\t%.2lf\n", dim_conf->dimmed_pct);
}

static void log_dpms_conf(dpms_conf_t *dpms_conf) {
    fprintf(log_file, "\n### DPMS ###\n");
    fprintf(log_file, "* Timeouts:\t\tAC %d\tBATT %d\n", dpms_conf->timeout[ON_AC], dpms_conf->timeout[ON_BATTERY]);
}

static void log_scr_conf(screen_conf_t *screen_conf) {
    fprintf(log_file, "\n### SCREEN ###\n");
    fprintf(log_file, "* Timeouts:\t\tAC %d\tBATT %d\n", screen_conf->timeout[ON_AC], screen_conf->timeout[ON_BATTERY]);
    fprintf(log_file, "* Contrib:\t\t%.2lf\n", screen_conf->contrib);
    fprintf(log_file, "* Samples:\t\t%d\n", screen_conf->samples);
}

static void log_inh_conf(inh_conf_t *inh_conf) {
    fprintf(log_file, "\n### INHIBIT ###\n");
    fprintf(log_file, "* Docked:\t\t%s\n", inh_conf->inhibit_docked ? "Enabled" : "Disabled");
    fprintf(log_file, "* PowerManagement:\t\t%s\n", inh_conf->inhibit_pm ? "Enabled" : "Disabled");
    fprintf(log_file, "* Backlight:\t\t%s\n", inh_conf->inhibit_bl ? "Enabled" : "Disabled");
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
        
        fprintf(log_file, "\n### GENERIC ###\n");
        fprintf(log_file, "* Verbose (debug):\t\t%s\n", conf.verbose ? "Enabled" : "Disabled");
        fprintf(log_file, "* ResumeDelay:\t\t%d\n", conf.resumedelay);
        
        if (!conf.bl_conf.disabled) {
            log_bl_conf(&conf.bl_conf);
            log_sens_conf(&conf.sens_conf);
        }
        
        if (!conf.kbd_conf.disabled) {
            log_kbd_conf(&conf.kbd_conf);
        }
        
        if (!conf.gamma_conf.disabled) {
           log_gamma_conf(&conf.gamma_conf);
        }
        
        log_daytime_conf(&conf.day_conf);
        
        if (!conf.dim_conf.disabled) {
            log_dim_conf(&conf.dim_conf);
        }
        
        if (!conf.dpms_conf.disabled) {
           log_dpms_conf(&conf.dpms_conf);
        }
        
        if (!conf.screen_conf.disabled) {
           log_scr_conf(&conf.screen_conf);
        }
        
        if (!conf.inh_conf.disabled) {
            log_inh_conf(&conf.inh_conf);
        }
        
        fprintf(log_file, "\n");
        fflush(log_file);
    }
}

void log_message(const char *filename, int lineno, const char type, const char *log_msg, ...) {
    // Debug and plot message only in verbose mode
    if ((type != LOG_DEBUG && type != LOG_PLOT) || conf.verbose) {
        va_list file_args, args;

        va_start(file_args, log_msg);
        va_copy(args, file_args);

        if (log_file) {
            if (type != LOG_PLOT) {
                time_t t = time(NULL);
                struct tm *tm = localtime(&t);
                fprintf(log_file, "(%c)[%02d:%02d:%02d]{%s:%d}\t", type, tm->tm_hour, tm->tm_min, tm->tm_sec, filename, lineno);
            }
            vfprintf(log_file, log_msg, file_args);
            fflush(log_file);
        }

        /* In case of error, log to stdout too */
        FILE *out = stdout;
        if (type == LOG_ERR) {
            out = stderr;
        }

        vfprintf(out, log_msg, args);
        va_end(args);
        va_end(file_args);
    }
}

void close_log(void) {
    if (log_file) {
        flock(fileno(log_file), LOCK_UN);
        fclose(log_file);
    }
}
