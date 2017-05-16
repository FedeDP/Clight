#pragma once

#include "log.h"

void init_modules(const enum modules module);
void init_module(int fd, enum modules module, void (*cb)(void));
void set_self_deps(struct self_t *self);
void disable_module(const enum modules module);
void poll_cb(const enum modules module);
void change_dep_type(const enum modules mod, const enum modules mod_dep, const enum dep_type type);
void destroy_modules(const enum modules module);
