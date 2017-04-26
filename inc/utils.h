#pragma once

#include "log.h"
#include <sys/timerfd.h>

int start_timer(int clockid, int initial_ns, int initial_s);
void set_timeout(int sec, int nsec, int fd, int flag);
void init_modules(const enum modules module);
void init_module(int fd, enum modules module, void (*cb)(void));
void set_self_deps(struct self_t *self);
void disable_module(const enum modules module);
void poll_cb(const enum modules module);
void destroy_modules(const enum modules module);
