#include <sys/file.h>
#include <sys/stat.h>
#include "commons.h"
#include "utils.h"

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

#define X_CONF(tp, name, def) \
    do { \
        if (strcmp(#name, "disabled") == 0) break; \
        const char *fmt = \
        _Generic((conf->name), \
            int: "* %s: \t\t%d\n", \
            double: "* %s: \t\t%.2lf\n", \
            const char *: "* %s: \t\t%s\n", \
            bool_to_int: "* %s: \t\t%d\n"); \
            \
        fprintf(log_file, fmt, #name, conf->name); \
    } while(0);

static void log_bl_smooth(bl_smooth_t *smooth, const char *prefix) {
    fprintf(log_file, "* no_smooth_transition[%s]:\t\t%d\n", prefix, smooth->no_smooth);
    fprintf(log_file, "* trans_step[%s]:\t\t%.2lf\n", prefix, smooth->trans_step);
    fprintf(log_file, "* trans_timeout[%s]:\t\t%d\n", prefix, smooth->trans_timeout);
    fprintf(log_file, "* trans_fixed[%s]:\t\t%d\n", prefix, smooth->trans_fixed);
}

static void log_generic_conf(conf_t *conf) {
    fprintf(log_file, "\n### GENERIC ###\n");
    
    X_GENERIC_CONF
}

static void log_bl_conf(bl_conf_t *conf) {
    fprintf(log_file, "\n### BACKLIGHT ###\n");
    
    fprintf(log_file, "* timeouts[DAY]:\t\tAC %d\tBATT %d\n", conf->timeout[ON_AC][DAY], conf->timeout[ON_BATTERY][DAY]);
    fprintf(log_file, "* timeouts[NIGHT]:\t\tAC %d\tBATT %d\n", conf->timeout[ON_AC][NIGHT], conf->timeout[ON_BATTERY][NIGHT]);
    fprintf(log_file, "* timeouts[EVENT]:\t\tAC %d\tBATT %d\n", conf->timeout[ON_AC][SIZE_STATES], conf->timeout[ON_BATTERY][SIZE_STATES]);
    
    X_BL_CONF
}

static void log_sens_conf(sensor_conf_t *conf) {
    fprintf(log_file, "\n### SENSOR ###\n");
    fprintf(log_file, "* captures:\t\tAC %d\tBATT %d\n", conf->num_captures[ON_AC], conf->num_captures[ON_BATTERY]);
   
    X_SENS_CONF
}

static void log_kbd_conf(kbd_conf_t *conf) {
    fprintf(log_file, "\n### KEYBOARD ###\n");
    fprintf(log_file, "* timeouts:\t\tAC %d\tBATT %d\n", conf->timeout[ON_AC], conf->timeout[ON_BATTERY]);
    
    X_KBD_CONF
}

static void log_gamma_conf(gamma_conf_t *conf) {
    fprintf(log_file, "\n### GAMMA ###\n");
    fprintf(log_file, "* temp[DAY]:\t\t%d\n", conf->temp[DAY]);
    fprintf(log_file, "* temp[NIGHT]:\t\t%d\n", conf->temp[NIGHT]);
    
    X_GAMMA_CONF
}

static void log_daytime_conf(daytime_conf_t *conf) {
    fprintf(log_file, "\n### DAYTIME ###\n");
    if (conf->loc.lat != LAT_UNDEFINED && conf->loc.lon != LON_UNDEFINED) {
        fprintf(log_file, "* location:\t\t%.2lf\t%.2lf\n", conf->loc.lat, conf->loc.lon);
    } else {
        fprintf(log_file, "* location:\t\tUnset\n");
    }
    fprintf(log_file, "* sunrise:\t\t%s\n", is_string_empty(conf->day_events[SUNRISE]) ? "Unset" : conf->day_events[SUNRISE]);
    fprintf(log_file, "* sunset:\t\t%s\n", is_string_empty(conf->day_events[SUNSET]) ? "Unset" : conf->day_events[SUNSET]);
    fprintf(log_file, "* sunrise offset:\t\t%d\n", conf->events_os[SUNRISE]);
    fprintf(log_file, "* sunset offset:\t\t%d\n", conf->events_os[SUNSET]);
    
    X_DAYTIME_CONF
}

static void log_dim_conf(dimmer_conf_t *conf) {
    fprintf(log_file, "\n### DIMMER ###\n");
    fprintf(log_file, "* timeouts:\t\tAC %d\tBATT %d\n", conf->timeout[ON_AC], conf->timeout[ON_BATTERY]);
    log_bl_smooth(&conf->smooth[ENTER], "ENTER");
    log_bl_smooth(&conf->smooth[EXIT], "EXIT");
    
    X_DIMMER_CONF
}

static void log_dpms_conf(dpms_conf_t *conf) {
    fprintf(log_file, "\n### DPMS ###\n");
    fprintf(log_file, "* timeouts:\t\tAC %d\tBATT %d\n", conf->timeout[ON_AC], conf->timeout[ON_BATTERY]);
    
    X_DPMS_CONF
}

static void log_scr_conf(screen_conf_t *conf) {
    fprintf(log_file, "\n### SCREEN ###\n");
    fprintf(log_file, "* timeouts:\t\tAC %d\tBATT %d\n", conf->timeout[ON_AC], conf->timeout[ON_BATTERY]);
    
    X_SCREEN_CONF
}

static void log_inh_conf(inh_conf_t *conf) {
    fprintf(log_file, "\n### INHIBIT ###\n");
   
    X_INH_CONF
}

#undef X_CONF

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
        
        log_generic_conf(&conf);
        
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
