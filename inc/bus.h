#pragma once

#include <systemd/sd-bus.h>
#include "timer.h"
#include "userbus.h"

struct bus_cb {
    enum modules module;
    void (*cb)(void);
};

/*
 * Object wrapper for bus calls
 */
struct bus_args {
    const char *service;
    const char *path;
    const char *interface;
    const char *member;
    enum bus_type type;
};

void set_bus_self(void);
void bus_callback(void);
int call(void *userptr, const char *userptr_type, const struct bus_args *args, const char *signature, ...);
int add_match(const struct bus_args *a, sd_bus_slot **slot, sd_bus_message_handler_t cb);
int set_property(const struct bus_args *a, const char type, const char *value);
int get_property(const struct bus_args *a, const char *type, void *userptr);
void add_mod_callback(const struct bus_cb cb);
