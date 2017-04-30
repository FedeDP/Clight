#pragma once

#include "commons.h"

#define INFO(msg, ...) log_message(__FILE__, __LINE__, 'I', msg, ##__VA_ARGS__)
#define WARN(msg, ...) log_message(__FILE__, __LINE__, 'W', msg, ##__VA_ARGS__)
#define ERROR(msg, ...) log_message(__FILE__, __LINE__, 'E', msg, ##__VA_ARGS__)

void open_log(void);
void log_conf(void);
void log_message(const char *filename, int lineno, const char type, const char *log_msg, ...);
void close_log(void);
