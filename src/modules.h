#pragma once

#include <timer.h>

#define _ctor_ __attribute__((constructor))

#define MODULE(module) \
    static void init(void); \
    static int check(void); \
    static void destroy(void); \
    static void callback(void); \
    static void _ctor_ set_module_self(void) { \
        *(int *)&self.idx = module; \
        self.name = #module; \
        modules[self.idx].self = &self; \
        modules[self.idx].init = init; \
        modules[self.idx].check = check; \
        modules[self.idx].poll_cb = callback; \
        modules[self.idx].destroy = destroy; \
        set_self_deps(&self); \
        state.needed_functional_modules += self.functional_module; \
    }

#define INIT_MOD(fd, ...) init_module(fd, self.idx, ##__VA_ARGS__, (void *)0)

void init_modules(const enum modules module);
void init_module(int fd, enum modules module, ...);
int is_disabled(const enum modules module);
int is_running(const enum modules module);
int is_started_disabled(const enum modules module);
int is_idle(const enum modules module);
int is_destroyed(const enum modules module);
void set_self_deps(struct self_t *self);
void poll_cb(const enum modules module);
void change_dep_type(const enum modules mod, const enum modules mod_dep, const enum dep_type type);
void destroy_modules(void);
