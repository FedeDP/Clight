#include "../inc/bus.h"

static void free_bus_structs(sd_bus_error *err, sd_bus_message *m, sd_bus_message *reply);
static int check_err(int r, sd_bus_error *err);

static sd_bus *bus;

void init_bus(void) {
    int r = sd_bus_open_system(&bus);
    if (r < 0) {
        fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
        state.quit = 1;
    }
}

void bus_call(void *userptr, const char *userptr_type, const char *method, const char *signature, ...) {
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *m = NULL, *reply = NULL;
    
    int r = sd_bus_message_new_method_call(bus, &m, "org.clight.backlight", "/org/clight/backlight", "org.clight.backlight", method);
    if (check_err(r, &error)) {
        goto finish;
    }
    
    va_list args;
    va_start(args, signature);
     
    int i = 0;
    char *s;
    int val;
    while (signature[i] != '\0') {
        switch (signature[i]) {
            case 's':
                s = va_arg(args, char *);
                r = sd_bus_message_append_basic(m, 's', s);
                if (check_err(r, &error)) {
                    goto finish;
                }
                break;
            case 'i':
                val = va_arg(args, int);
                r = sd_bus_message_append_basic(m, 'i', &val);
                if (check_err(r, &error)) {
                    goto finish;
                }
                break;
            default:
                fprintf(stderr, "Wrong signature in bus call: %c.\n", signature[i]);
                break;
        }
        i++;
    }
    
    va_end(args);
    r = sd_bus_call(bus, m, 0, &error, &reply);
    if (check_err(r, &error)) {
        goto finish;
    }
    
    /* Parse the response message */
    r = sd_bus_message_read(reply, userptr_type, userptr);
    check_err(r, &error);
    
finish:
    free_bus_structs(&error, m, reply);
}

static void free_bus_structs(sd_bus_error *err, sd_bus_message *m, sd_bus_message *reply) {
    if (err) {
        sd_bus_error_free(err);
    }
    
    if (m) {
        sd_bus_message_unref(m);
    }
    
    if (reply) {
        sd_bus_message_unref(reply);
    }
}

static int check_err(int r, sd_bus_error *err) {
    if (r < 0) {
        if (err->message) {
            fprintf(stderr, "%s\n", err->message);
        }
        /* Don't leave for ebusy errors */
        if (r != -EBUSY) {
            state.quit = 1;
        }
    }
    return r < 0;
}

void destroy_bus(void) {
    if (bus) {
        sd_bus_flush_close_unref(bus);
    }
}
