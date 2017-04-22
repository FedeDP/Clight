#pragma once

#include "log.h"
#include <sys/timerfd.h>

int start_timer(int clockid, int initial_timeout);
void set_timeout(int sec, int nsec, int fd, int flag);
void init_module(int fd, enum modules module, void (*cb)(void), void (*destroy_func)(void), const char *module_name);
