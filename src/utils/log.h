#pragma once

#include <setjmp.h>

/* ERROR macro will leave clight by calling modules_quit; thus it is not exposed to public header */
#define ERROR(msg, ...) \
do { \
    log_message(__FILENAME__, __LINE__, 'E', msg, ##__VA_ARGS__); \
    if (state.looping) modules_quit(EXIT_FAILURE); \
    else longjmp(state.quit_buf, EXIT_FAILURE); \
} while (0)

/* Used to plot backlight curves to log without headers */
#define PLOT(msg, ...)  log_message(__FILENAME__, __LINE__, 'P', msg, ##__VA_ARGS__)

void open_log(void);
void log_conf(void);
void close_log(void);
