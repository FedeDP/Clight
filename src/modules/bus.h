#pragma once

#include <systemd/sd-bus.h>
#include "timer.h"

#define CLIGHTD_SERVICE "org.clightd.clightd"

/* Bus types */
enum bus_type { SYSTEM_BUS, USER_BUS };

/*
 * Object wrapper for bus calls
 */
struct bus_args {
    const char *service;
    const char *path;
    const char *interface;
    const char *member;
    enum bus_type type;
    const char *caller;
    sd_bus *bus;
};

#define BUS_ARG(name, ...)      struct bus_args name = {__VA_ARGS__, __func__};
#define USERBUS_ARG(name, ...)  BUS_ARG(name, __VA_ARGS__, USER_BUS);
#define SYSBUS_ARG(name, ...)   BUS_ARG(name, __VA_ARGS__, SYSTEM_BUS);

int call(void *userptr, const char *userptr_type, const struct bus_args *args, const char *signature, ...);
int add_match(const struct bus_args *a, sd_bus_slot **slot, sd_bus_message_handler_t cb);
int set_property(const struct bus_args *a, const char type, const void *value);
int get_property(const struct bus_args *a, const char *type, void *userptr, int size);
sd_bus *get_user_bus(void);
