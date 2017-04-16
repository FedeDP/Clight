#pragma once

#include "commons.h"

#define INFO(msg, ...) log_message('I', msg, ##__VA_ARGS__)
#define ERROR(msg, ...) log_message('E', msg, ##__VA_ARGS__)

void open_log(void);
void log_conf(void);
void log_message(const char type, const char *log_msg, ...);
void close_log(void);
