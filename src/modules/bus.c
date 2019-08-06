#include <bus.h>

#define GET_BUS(a)  sd_bus *tmp = a->type == USER_BUS ? userbus : sysbus; if (!tmp) { return -1; }

static void free_bus_structs(sd_bus_error *err, sd_bus_message *m, sd_bus_message *reply);
static int check_err(int r, sd_bus_error *err, const char *caller);

static sd_bus *sysbus, *userbus;

MODULE("BUS");

static void module_pre_start(void) {
    sd_bus_default_system(&sysbus);
    sd_bus_default_user(&userbus);
}

static void init(void) {
    if (!sysbus) {
        ERROR("BUS: Failed to connect to system bus.\n");
    }
    
    if (!userbus) {
        ERROR("BUS: Failed to connect to user bus\n");
    }
    
    int bus_fd = sd_bus_get_fd(sysbus);
    int userbus_fd = sd_bus_get_fd(userbus);

    m_register_fd(bus_fd, false, sysbus);
    m_register_fd(userbus_fd, false, userbus);
}

static bool check(void) {
    return true;
}

static bool evaluate(void) {
    return true;
}

static void destroy(void) {
    if (sysbus) {
        sd_bus_flush_close_unref(sysbus);
    }
    if (userbus) {
        sd_bus_flush_close_unref(userbus);
    }
}

static void receive(const msg_t *const msg, const void* userdata) {
    if (!msg->is_pubsub) {
        sd_bus *b = (sd_bus *)msg->fd_msg->userptr;
        int r;
        do {
            r = sd_bus_process(b, NULL);
        } while (r > 0);
    }
}

/*
 * Call a method on bus and store its result of type userptr_type in userptr.
 */
int call(void *userptr, const char *userptr_type, const struct bus_args *a, const char *signature, ...) {
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *m = NULL, *reply = NULL;
    GET_BUS(a);

    int r = sd_bus_message_new_method_call(tmp, &m, a->service, a->path, a->interface, a->member);
    if (check_err(r, &error, a->caller)) {
        goto finish;
    }

    r = sd_bus_message_set_expect_reply(m, userptr != NULL);
    if (check_err(r, &error, a->caller)) {
        goto finish;
    }

    if (signature) {
        va_list args;
        va_start(args, signature);

#if LIBSYSTEMD_VERSION >= 234
        sd_bus_message_appendv(m, signature, args);
#else
        int i = 0;
        while (signature[i] != '\0') {
            switch (signature[i]) {
                case SD_BUS_TYPE_STRING: {
                    char *val = va_arg(args, char *);
                    r = sd_bus_message_append_basic(m, signature[i], val);
                }
                break;

                case SD_BUS_TYPE_INT32:
                case SD_BUS_TYPE_UINT32:
                case SD_BUS_TYPE_BOOLEAN: {
                    int val = va_arg(args, int);
                    r = sd_bus_message_append_basic(m, signature[i], &val);
                }
                break;

                case SD_BUS_TYPE_DOUBLE: {
                    double val = va_arg(args, double);
                    r = sd_bus_message_append_basic(m, signature[i], &val);
                }
                break;

                case SD_BUS_TYPE_STRUCT_BEGIN: {
                    char *ptr = strchr(signature + i, SD_BUS_TYPE_STRUCT_END);
                    if (ptr) {
                        char str[30] = {0};
                        strncpy(str, signature + i + 1, strlen(signature + i + 1) - strlen(ptr));
                        r = sd_bus_message_open_container(m, SD_BUS_TYPE_STRUCT, str);
                    }
                    break;
                }

                case SD_BUS_TYPE_STRUCT_END:
                    r = sd_bus_message_close_container(m);
                    break;

                default:
                    WARN("BUS: Wrong signature in bus call: %c.\n", signature[i]);
                    break;
            }

            if (check_err(r, &error, a->caller)) {
                goto finish;
            }
            i++;
        }
#endif
        va_end(args);
    }
    /* Check if we need to wait for a response message */
    if (userptr != NULL) {
        r = sd_bus_call(tmp, m, 0, &error, &reply);
        if (check_err(r, &error, a->caller)) {
            goto finish;
        }

        /*
         * Fix for new Clightd interface for CaptureSensor and IsSensorAvailable:
         * they will now return used interface too. We don't need it.
         */
        if (strlen(userptr_type) > 1 && !strcmp(a->service, CLIGHTD_SERVICE)) {
            const char *unused = NULL;
            sd_bus_message_read(reply, "s", &unused);
            userptr_type++;
        }
        if (userptr_type[0] == 'o') {
            const char *obj = NULL;
            r = sd_bus_message_read(reply, "o", &obj);
            if (r >= 0) {
                strncpy(userptr, obj, PATH_MAX);
            }
        } else if (userptr_type[0] == 'a') {
            const void *data = NULL;
            size_t length;
            r = sd_bus_message_read_array(reply, userptr_type[1], &data, &length);
            memcpy(userptr, data, length);
        } else {
            r = sd_bus_message_read(reply, userptr_type, userptr);
        }
        r = check_err(r, &error, a->caller);
    } else {
        r = sd_bus_send(tmp, m, NULL);
        r = check_err(r, &error, a->caller);
    }

finish:
    free_bus_structs(&error, m, reply);
    return r;
}

/*
 * Add a match on bus on certain signal for cb callback
 */
int add_match(const struct bus_args *a, sd_bus_slot **slot, sd_bus_message_handler_t cb) {
    GET_BUS(a);

#if LIBSYSTEMD_VERSION >= 237
    int r = sd_bus_match_signal(tmp, slot, a->service, a->path, a->interface, a->member, cb, &state);
#else
    char match[500] = {0};
    snprintf(match, sizeof(match), "type='signal', sender='%s', interface='%s', member='%s', path='%s'", a->service, a->interface, a->member, a->path);
    int r = sd_bus_add_match(tmp, slot, match, cb, &state);
#endif
    return check_err(r, NULL, a->caller);
}

/*
 * Set property of type "type" value to "value". It correctly handles 'u' and 's' types.
 */
int set_property(const struct bus_args *a, const char type, const void *value) {
    GET_BUS(a);
    sd_bus_error error = SD_BUS_ERROR_NULL;
    int r = 0;

    switch (type) {
        case SD_BUS_TYPE_UINT32:
            r = sd_bus_set_property(tmp, a->service, a->path, a->interface, a->member, &error, "u", *(unsigned int *)value);
            break;
        case SD_BUS_TYPE_STRING:
            r = sd_bus_set_property(tmp, a->service, a->path, a->interface, a->member, &error, "s", value);
            break;
        default:
            WARN("BUS: Wrong signature in bus call: %c.\n", type);
            break;
    }
    r = check_err(r, &error, a->caller);
    free_bus_structs(&error, NULL, NULL);
    return r;
}

/*
 * Get a property of type "type" into userptr.
 */
int get_property(const struct bus_args *a, const char *type, void *userptr) {
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *m = NULL;
    GET_BUS(a);

    int r = sd_bus_get_property(tmp, a->service, a->path, a->interface, a->member, &error, &m, type);
    if (check_err(r, &error, a->caller)) {
        goto finish;
    }
    if (!strcmp(type, "o") || !strcmp(type, "s")) {
        const char *obj = NULL;
        r = sd_bus_message_read(m, type, &obj);
        if (r >= 0) {
            strncpy(userptr, obj, PATH_MAX);
        }
    } else {
        r = sd_bus_message_read(m, type, userptr);
    }
    r = check_err(r, NULL, a->caller);

finish:
    free_bus_structs(&error, m, NULL);
    return r;
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
 * Check any error. Do not leave for EBUSY, EPERM and EHOSTUNREACH errors.
 * Only leave for EHOSTUNREACH if from clightd. (it can't be missing).
 * Will return 1 if an error happened, 0 otherwise.
 * Note that this function will only return 1 if a not fatal error happened (ie: WARN has been called),
 * as ERROR() macro does a longjmp
 */
static int check_err(int r, sd_bus_error *err, const char *caller) {
    if (r < 0) {
        DEBUG("BUS: %s(): %s\n", caller, err && err->message ? err->message : strerror(-r));
    }
    /* -1 on error, 0 ok */
    return -(r < 0);
}

sd_bus *get_user_bus(void) {
    return userbus;
}
