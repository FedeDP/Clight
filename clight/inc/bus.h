#pragma once

#include <systemd/sd-bus.h>
#include <stdarg.h>

#include "commons.h"

/*
 * Object wrapper for bus calls
 */
struct bus_args {
    const char *service;
    const char *path;
    const char *interface;
    const char *member;
};

sd_bus *bus;

void init_bus(void);
void bus_call(void *userptr, const char *userptr_type, const struct bus_args *args, const char *signature, ...);
void add_match(const struct bus_args *a, int (*cb) (sd_bus_message *, void *, sd_bus_error *));
void set_property(const struct bus_args *a, const char type, const char *value);
void get_property(const struct bus_args *a, const char *type, void *userptr);
int check_err(int r, sd_bus_error *err);
void destroy_bus(void);
