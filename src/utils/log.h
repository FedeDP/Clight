#pragma once

#include <setjmp.h>

/* ERROR macro will leave clight by calling longjmp; thus it is not exposed to public header */
#define ERROR(msg, ...) \
do { \
    log_message(__FILENAME__, __LINE__, 'E', msg, ##__VA_ARGS__); \
    longjmp(state.quit_buf, ERR_QUIT); \
} while (0)

void open_log(void);
void log_conf(void);
void close_log(void);
