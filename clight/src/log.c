#include "../inc/log.h"
#include <pwd.h>

static FILE *log_file;

void open_log(void) {
    const char log_name[] = "clight.log";

    char log_path[PATH_MAX + 1] = {0};

    snprintf(log_path, PATH_MAX, "%s/.%s", getpwuid(getuid())->pw_dir, log_name);
    log_file = fopen(log_path, "w");
    if (!log_file) {
        ERROR("%s\n", strerror(errno));
    }
}

void log_conf(void) {
    if (log_file) {
        time_t t = time(NULL);

        fprintf(log_file, "Clight\n");
        fprintf(log_file, "Time: %s", ctime(&t));
        fprintf(log_file, "\nStarting options:\n");
        fprintf(log_file, "* Number of captures: %d\n", conf.num_captures);
        fprintf(log_file, "* Daily timeout: %d\n", conf.timeout[DAY]);
        fprintf(log_file, "* Nightly timeout: %d\n", conf.timeout[NIGHT]);
        fprintf(log_file, "* Webcam device: %s\n", conf.dev_name);
        fprintf(log_file, "* Backlight path: %s\n", conf.screen_path);
        fprintf(log_file, "* Daily screen temp: %d\n", conf.temp[DAY]);
        fprintf(log_file, "* Nightly screen temp: %d\n", conf.temp[NIGHT]);
        fprintf(log_file, "* Smooth transitions: %d\n", conf.smooth_transition);
        fprintf(log_file, "* Latitude: %.2lf\n", conf.lat);
        fprintf(log_file, "* Longitude: %.2lf\n\n", conf.lon);
    }
}

void log_message(const char type, const char *log_msg, ...) {
    va_list file_args, args;

    va_start(file_args, log_msg);
    va_copy(args, file_args);
    if (log_file) {
        fprintf(log_file, "(%c) ", type);
        vfprintf(log_file, log_msg, file_args);
        fflush(log_file);
    }
#ifdef DEBUG
    vprintf(log_msg, args);
#else
    /* in case of error, write it to stderr */
    if (type == 'E') {
        vfprintf(stderr, log_msg, args);
    }
#endif
    va_end(args);
    va_end(file_args);
}

void close_log(void) {
    if (log_file) {
        fclose(log_file);
    }
}
