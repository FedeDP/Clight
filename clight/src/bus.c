#include "../inc/bus.h"

static void free_bus_structs(sd_bus_error *err, sd_bus_message *m, sd_bus_message *reply);

/*
 * Open our bus
 */
void init_bus(void) {
    int r = sd_bus_open_system(&bus);
    if (r < 0) {
        ERROR("Failed to connect to system bus: %s\n", strerror(-r));
        state.quit = 1;
    } else {
        INFO("Bus support started.\n");
    }
}

/*
 * Calls a method on bus and store its result of type userptr_type in userptr.
 * Note that this is a variadic function and will correctly handle 's' and 'i' signature types.
 * Follow: https://github.com/systemd/systemd/issues/5654 (i need to propose a patch there)
 */
void bus_call(void *userptr, const char *userptr_type, const struct bus_args *a, const char *signature, ...) {
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *m = NULL, *reply = NULL;

    int r = sd_bus_message_new_method_call(bus, &m, a->service, a->path, a->interface, a->member);
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
                ERROR("Wrong signature in bus call: %c.\n", signature[i]);
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
    if (userptr != NULL) {
        if (!strncmp(userptr_type, "o", strlen("o"))) {
            const char *obj = NULL;
            r = sd_bus_message_read(reply, userptr_type, &obj);
            if (r >= 0) {
                strncpy(userptr, obj, PATH_MAX);
            }
        } else {
            r = sd_bus_message_read(reply, userptr_type, userptr);
        }
        check_err(r, &error);
    }

finish:
    free_bus_structs(&error, m, reply);
}

/*
 * Add a match on bus on certain signal for cb callback
 */
void add_match(const struct bus_args *a, sd_bus_message_handler_t cb) {
    char match[500] = {0};
    snprintf(match, sizeof(match), "type='signal',interface='%s', member='%s', path='%s'", a->interface, a->member, a->path);
    int r = sd_bus_add_match(bus, NULL, match, cb, NULL);
    check_err(r, NULL);
}

/*
 * Set property of type "type" value to "value". It correctly handles 'u' and 's' types.
 */
void set_property(const struct bus_args *a, const char type, const char *value) {
    sd_bus_error error = SD_BUS_ERROR_NULL;
    int r = 0;

    switch (type) {
        case 'u':
            r = sd_bus_set_property(bus, a->service, a->path, a->interface, a->member, &error, "u", atoi(value));
            break;
        case 's':
            r = sd_bus_set_property(bus, a->service, a->path, a->interface, a->member, &error, "s", value);
            break;
        default:
            ERROR("Wrong signature in bus call: %c.\n", type);
            break;
    }
    check_err(r, &error);
    free_bus_structs(&error, NULL, NULL);
}

/*
 * Get a property of type "type" into userptr.
 */
void get_property(const struct bus_args *a, const char *type, void *userptr) {
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *m = NULL;

    int r = sd_bus_get_property(bus, a->service, a->path, a->interface, a->member, &error, &m, type);
    if (check_err(r, &error)) {
        goto finish;
    }

    if (!strcmp(type, "o")) {
        const char *obj = NULL;
        r = sd_bus_message_read(m, type, &obj);
        if (r >= 0) {
            strncpy(userptr, obj, PATH_MAX);
        }
    } else {
        r = sd_bus_message_read(m, type, userptr);
    }
    check_err(r, NULL);

finish:
    free_bus_structs(&error, m, NULL);
}

/*
 * Free used resources.
 */
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

/*
 * Check any error. Do not leave for EBUSY errors.
 */
int check_err(int r, sd_bus_error *err) {
    if (r < 0) {
        if (err->message) {
            ERROR("%s\n", err->message);
        } else {
            ERROR("%s\n", strerror(-r));
        }
        /* Don't leave for ebusy errors */
        if (r != -EBUSY) {
            state.quit = 1;
        }
    }
    return r < 0;
}

/*
 * Close bus.
 */
void destroy_bus(void) {
    if (bus) {
        sd_bus_flush_close_unref(bus);
    }
    INFO("Bus destroyed.\n");
}
