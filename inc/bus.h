#pragma once

#include <systemd/sd-bus.h>

#include "utils.h"

/*
 * Object wrapper for bus calls
 */
struct bus_args {
    const char *service;
    const char *path;
    const char *interface;
    const char *member;
};

void set_bus_self(void);
void bus_call(void *userptr, const char *userptr_type, const struct bus_args *args, const char *signature, ...);
void add_match(const struct bus_args *a, sd_bus_message_handler_t cb);
void set_property(const struct bus_args *a, const char type, const char *value);
void get_property(const struct bus_args *a, const char *type, void *userptr);
