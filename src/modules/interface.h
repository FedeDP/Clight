#pragma once

#include "bus.h"

#define VALIDATE_PARAMS(m, signature, ...) \
    int r = sd_bus_message_read(m, signature, __VA_ARGS__); \
    if (r < 0) { \
        WARN("Failed to parse parameters: %s\n", strerror(-r)); \
        return r; \
    }

#define API_CONCAT(prefix, apiName, suffix) prefix##_##apiName##_##suffix
#define API(apiName, vtable, config) \
    static sd_bus_slot *API_CONCAT(apiName, api, slot); \
    static void API_CONCAT(init, apiName, api)(void) { \
        const char conf_path[] = "/org/clight/clight/Conf/" # apiName; \
        const char conf_interface[] = "org.clight.clight.Conf." # apiName; \
        sd_bus *userbus = get_user_bus(); \
        int r = sd_bus_add_object_vtable(userbus, \
                                        &API_CONCAT(apiName, api, slot), \
                                        conf_path, \
                                        conf_interface, \
                                        vtable, \
                                        &config); \
        if (r < 0) { \
            WARN("Could not create dbus interface '%s': %s\n", conf_interface, strerror(-r)); \
        } \
    } \
    static void API_CONCAT(deinit, apiName, api)(void) { \
        if (API_CONCAT(apiName, api, slot)) { sd_bus_slot_unrefp(&API_CONCAT(apiName, api, slot)); } \
    }

int set_timeouts(sd_bus *bus, const char *path, const char *interface, const char *property,
                        sd_bus_message *value, void *userdata, sd_bus_error *error);
int get_curve(sd_bus *bus, const char *path, const char *interface, const char *property,
                     sd_bus_message *reply, void *userdata, sd_bus_error *error);
int set_curve(sd_bus *bus, const char *path, const char *interface, const char *property,
                     sd_bus_message *value, void *userdata, sd_bus_error *error);
int get_location(sd_bus *bus, const char *path, const char *interface, const char *property,
                        sd_bus_message *reply, void *userdata, sd_bus_error *error);
int set_location(sd_bus *bus, const char *path, const char *interface, const char *property,
                        sd_bus_message *value, void *userdata, sd_bus_error *error);
