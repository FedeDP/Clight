#pragma once

/* ERROR macro will leave clight by calling modules_quit; thus it is not exposed to public header */
#define ERROR(msg, ...) \
do { \
    log_message(__FILENAME__, __LINE__, 'E', msg, ##__VA_ARGS__); \
    modules_quit(EXIT_FAILURE); \
} while (0)

void open_log(void);
void log_conf(void);
void close_log(void);
