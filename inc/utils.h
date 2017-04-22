#pragma once

#include "log.h"
#include <sys/timerfd.h>

int start_timer(int clockid, int initial_timeout);
void set_timeout(int sec, int nsec, int fd, int flag);
void init_modules(const int limit);
void init_module(int fd, enum modules module, void (*cb)(void));
void set_deps(struct self_t *self);
void destroy_modules(const int limit);
