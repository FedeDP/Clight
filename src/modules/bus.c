#include <bus.h>

static void run_callbacks(struct bus_match_data *data);
static void free_bus_structs(sd_bus_error *err, sd_bus_message *m, sd_bus_message *reply);
static int check_err(int r, sd_bus_error *err, const char *caller);

struct bus_callback {
    int num_callbacks;
    struct bus_cb *callbacks;
};

static struct bus_callback _cb;
static sd_bus *sysbus, *userbus;
static struct self_t self;

MODULE(BUS);

/*
 * Open system bus and start listening on its fd
 */
static void init(void) {
    int r = sd_bus_default_system(&sysbus);
    if (r < 0) {
        ERROR("Failed to connect to system bus: %s\n", strerror(-r));
    }
    // let main poll listen on bus events
    int bus_fd = sd_bus_get_fd(sysbus);
    INIT_MOD(bus_fd);

}

static int check(void) {
    return 0; /* Skeleton function needed for modules interface */
}

/*
 * Close bus
 */
static void destroy(void) {
    if (sysbus) {
        sd_bus_flush_close_unref(sysbus);
    }
    if (_cb.callbacks) {
        free(_cb.callbacks);
    }
}

static void callback(void) {
    bus_callback(SYSTEM);
}

/*
 * Callback for bus events
 */
void bus_callback(const enum bus_type type) {
    sd_bus *cb_bus = type == USER ? userbus : sysbus;
    int r;
    do {
        /* reset bus_cb_idx to impossible state */
        state.userdata.bus_mod_idx = MODULES_NUM;
        r = sd_bus_process(cb_bus, NULL);
        /* check if any match changed bus_cb_idx, then call correct callback */
        if (state.userdata.bus_mod_idx != MODULES_NUM) {
            /* 
             * Run callbacks before poll_cb, as poll_cb will call
             * started_cb, thus starting all of dependent modules;
             * some of these modules may hook a callback on this module
             * and that callback would be ran if poll_cb was before run_callbacks()
             */
            run_callbacks(&state.userdata);
            if (state.userdata.ptr) {
                free(state.userdata.ptr);
                state.userdata.ptr = NULL;
            }
            poll_cb(state.userdata.bus_mod_idx);
        }
    } while (r > 0);
}

/*
 * Call a method on bus and store its result of type userptr_type in userptr.
 */
int call(void *userptr, const char *userptr_type, const struct bus_args *a, const char *signature, ...) {
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *m = NULL, *reply = NULL;
    sd_bus *tmp = a->type == USER ? userbus : sysbus;

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
                    WARN("Wrong signature in bus call: %c.\n", signature[i]);
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
    sd_bus *tmp = a->type == USER ? userbus : sysbus;

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
    sd_bus *tmp = a->type == USER ? userbus : sysbus;
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
            WARN("Wrong signature in bus call: %c.\n", type);
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
    sd_bus *tmp = a->type == USER ? userbus : sysbus;

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

void add_mod_callback(const struct bus_cb *cb) {
    struct bus_cb *tmp = realloc(_cb.callbacks, sizeof(struct bus_cb) * (++_cb.num_callbacks));
    if (tmp) {
        _cb.callbacks = tmp;
        _cb.callbacks[_cb.num_callbacks - 1] = *cb;
    } else {
        free(_cb.callbacks);
        ERROR("%s\n", strerror(errno));
    }
}

static void run_callbacks(struct bus_match_data *data) {
    for (int i = 0; i < _cb.num_callbacks; i++) {
        if (_cb.callbacks[i].module == data->bus_mod_idx && is_running(data->bus_mod_idx)
            && (!_cb.callbacks[i].filter || strcasestr(data->bus_fn_name, _cb.callbacks[i].filter))) {
            _cb.callbacks[i].cb(data->ptr);
        }
    }
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
        WARN("%s(): %s\n", caller, err && err->message ? err->message : strerror(-r));
    }
    /* -1 on error, 0 ok */
    return -(r < 0);
}

sd_bus **get_user_bus(void) {
    return &userbus;
}
