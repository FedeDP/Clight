#pragma once

#include <systemd/sd-bus.h>
#include "timer.h"

#define CLIGHTD_SERVICE "org.clightd.clightd"

/* Bus types */
enum bus_type { SYSTEM_BUS, USER_BUS };

/* Bus reply read callback */
typedef int(*bus_recv_cb)(sd_bus_message *reply, const char *member, void *userdata);

/*
 * Object wrapper for bus calls
 */
typedef struct {
    const char *service;
    const char *path;
    const char *interface;
    const char *member;
    enum bus_type type;
    bus_recv_cb reply_cb;
    void *reply_userdata;
    const char *caller;
    sd_bus *bus;
    bool async; // ASYNC requests NEED a static/heap memory bus_args!!
} bus_args;

#define BUS_ARG(name, ...)      bus_args name = { __VA_ARGS__, __func__ };

/* Define a bus_args local variable to actually parse message response */
#define USERBUS_ARG_REPLY(name, cb, userdata, ...)  BUS_ARG(name, __VA_ARGS__, USER_BUS, cb, userdata);
#define SYSBUS_ARG_REPLY(name, cb, userdata, ...)   BUS_ARG(name, __VA_ARGS__, SYSTEM_BUS, cb, userdata);

/* Define a bus_args local variable to discard message response (ie: not needed to be parsed) */
#define USERBUS_ARG(name, ...)  USERBUS_ARG_REPLY(name, NULL, NULL, __VA_ARGS__);
#define SYSBUS_ARG(name, ...)   SYSBUS_ARG_REPLY(name, NULL, NULL, __VA_ARGS__);


int call(const bus_args *a, const char *signature, ...);
int add_match(const bus_args *a, sd_bus_slot **slot, sd_bus_message_handler_t cb);
int set_property(const bus_args *a, const char *type, const uintptr_t value);
int get_property(const bus_args *a, const char *type, void *userptr);
sd_bus *get_user_bus(void);
