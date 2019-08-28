#pragma once

#include "bus.h"

int idle_init(char *client, sd_bus_slot **slot, int timeout, sd_bus_message_handler_t handler);
int idle_get_client(char *client);
int idle_hook_update(char *client, sd_bus_slot **slot, sd_bus_message_handler_t handler);
int idle_set_timeout(char *client, int timeout);
int idle_client_start(char *client, int timeout);
int idle_client_stop(char *client);
int idle_client_destroy(char *client);
