#include "idler.h"

#define VALIDATE_CLIENT(cl) do { if (!client || !strlen(client)) return -1; } while (0);

static int idle_get_client(char *client);
static int idle_hook_update(char *client, sd_bus_slot **slot, sd_bus_message_handler_t handler);

int idle_init(char *client, sd_bus_slot **slot, int timeout, sd_bus_message_handler_t handler) {
    int r = idle_get_client(client);
    if (r < 0) {
        goto end;
    }
    r = idle_hook_update(client, slot, handler);
    if (r < 0) {
        goto end;
    }
    r = idle_set_timeout(client, timeout); // set timeout automatically calls client start

end:
    if (r < 0) {
        WARN("Clightd idle error.\n");
        *client = '\0'; // reset client making it useless
    }
    return -(r < 0);  // - 1 on error
}

static int idle_get_client(char *client) {
    SYSBUS_ARG(args, CLIGHTD_SERVICE, "/org/clightd/clightd/Idle", "org.clightd.clightd.Idle", "GetClient");
    return call(client, "o", &args, NULL);
}

static int idle_hook_update(char *client, sd_bus_slot **slot, sd_bus_message_handler_t handler) {
    SYSBUS_ARG(args, CLIGHTD_SERVICE, client, "org.clightd.clightd.Idle.Client", "Idle");
    return add_match(&args, slot, handler);
}

int idle_set_timeout(char *client, int timeout) {
    VALIDATE_CLIENT(client);
    
    int r = 0;
    if (timeout > 0) {
        SYSBUS_ARG(to_args, CLIGHTD_SERVICE, client, "org.clightd.clightd.Idle.Client", "Timeout");
        r = set_property(&to_args, 'u', &timeout);
        if (!state.inhibited) {
            /* Only start client if we are not inhibited */
            r += idle_client_start(client, timeout);
        }
    } else {
        r = idle_client_stop(client);
    }
    return r;
}

int idle_client_start(char *client, int timeout) {
    VALIDATE_CLIENT(client);
    
    if (timeout > 0) {
        SYSBUS_ARG(args, CLIGHTD_SERVICE, client, "org.clightd.clightd.Idle.Client", "Start");
        return call(NULL, NULL, &args, NULL);
    }
    return 0;
}

int idle_client_stop(char *client) {
    VALIDATE_CLIENT(client);
    
    SYSBUS_ARG(args, CLIGHTD_SERVICE, client, "org.clightd.clightd.Idle.Client", "Stop");
    return call(NULL, NULL, &args, NULL);
}

int idle_client_destroy(char *client) {
    VALIDATE_CLIENT(client);
    
    /* Properly stop client */
    idle_client_stop(client);

    SYSBUS_ARG(args, CLIGHTD_SERVICE, "/org/clightd/clightd/Idle", "org.clightd.clightd.Idle", "DestroyClient");
    return call(NULL, NULL, &args, "o", client);
}
