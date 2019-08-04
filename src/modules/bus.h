#pragma once

#include <timer.h>

#define FILL_MATCH(name) \
    state.userdata.bus_mod_idx = 0; \
    state.userdata.bus_fn_name = name; \
    state.userdata.ptr = NULL;

/* Use this to specify a name as filter for the hooks to be called */
#define FILL_MATCH_DATA_NAME(data, name) \
    FILL_MATCH(name) \
    state.userdata.ptr = malloc(sizeof(data)); \
    memcpy(state.userdata.ptr, &data, sizeof(data)); \

/* Use this to use function name as filter for the hooks to be called and pass additional data */
#define FILL_MATCH_DATA(data)   FILL_MATCH_DATA_NAME(data, __PRETTY_FUNCTION__)

/* Use this to use function name as filter for the hooks to be called without passing additional data */
#define FILL_MATCH_NONE()   FILL_MATCH(__PRETTY_FUNCTION__)
    
/* Bus types */
enum bus_type { SYSTEM_BUS, USER_BUS };

struct bus_cb {
    enum modules module;
    void (*cb)(const void *ptr);
    const char *filter;
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
    const char *caller;
};

#define BUS_ARG(name, ...)      struct bus_args name = {__VA_ARGS__, __func__};
#define USERBUS_ARG(name, ...)  BUS_ARG(name, __VA_ARGS__, USER_BUS);
#define SYSBUS_ARG(name, ...)   BUS_ARG(name, __VA_ARGS__, SYSTEM_BUS);

int call(void *userptr, const char *userptr_type, const struct bus_args *args, const char *signature, ...);
int add_match(const struct bus_args *a, sd_bus_slot **slot, sd_bus_message_handler_t cb);
int set_property(const struct bus_args *a, const char type, const void *value);
int get_property(const struct bus_args *a, const char *type, void *userptr);
sd_bus *get_user_bus(void);
