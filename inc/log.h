#pragma once

#include "commons.h"

#define DEBUG(msg, ...) if (conf.verbose) log_message(__FILE__, __LINE__, 'D', msg, ##__VA_ARGS__)
#define INFO(msg, ...) log_message(__FILE__, __LINE__, 'I', msg, ##__VA_ARGS__)
#define WARN(msg, ...) log_message(__FILE__, __LINE__, 'W', msg, ##__VA_ARGS__)
/* ERROR macro will leave clight by calling longjmp */
#define ERROR(msg, ...) \
do { \
    log_message(__FILE__, __LINE__, 'E', msg, ##__VA_ARGS__); \
    longjmp(state.quit_buf, ERR_QUIT); \
} while (0)

void open_log(void);
void log_conf(void);
void log_message(const char *filename, int lineno, const char type, const char *log_msg, ...);
void close_log(void);
