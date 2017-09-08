#include "../inc/bus.h"

static void init(void);
static int check(void);
static void destroy(void);
static void callback(void);
static void run_callbacks(const enum modules mod, const void *payload);
static void free_bus_structs(sd_bus_error *err, sd_bus_message *m, sd_bus_message *reply);
static int check_err(int r, sd_bus_error *err);

/* 
 * bus_mod_idx: setted in every module's match callback to their self.idx.
 * It is the idx of the module on which bus should call callbacks
 * stored in struct bus_cb *callbacks
 */
struct bus_callback {
    int num_callbacks;
    struct bus_match_data userdata;
    struct bus_cb *callbacks;
};

static struct bus_callback _cb;
static sd_bus *bus, *userbus;
static struct self_t self = {
    .name = "Bus",
    .idx = BUS,
    .standalone = 1,
    .enabled_single_capture = 1
};

void set_bus_self(void) {
    SET_SELF();
}

/*
 * Open our bus and start lisetining on its fd
 */
static void init(void) {
    int r = sd_bus_default_system(&bus);
    if (r < 0) {
        ERROR("Failed to connect to system bus: %s\n", strerror(-r));
    }
    // let main poll listen on bus events
    int bus_fd = sd_bus_get_fd(bus);
    INIT_MOD(bus_fd);
}

static int check(void) {
    return 0; /* Skeleton function needed for modules interface */
}

/*
 * Close bus
 */
static void destroy(void) {
    if (bus) {
        bus = sd_bus_flush_close_unref(bus);
    }
    if (_cb.callbacks) {
        free(_cb.callbacks);
    }
}

/*
 * Callback for bus events
 */
static void callback(void) {
    int r;
    do {
        /* reset bus_cb_idx to impossible state */
        _cb.userdata.bus_mod_idx = MODULES_NUM;
        r = sd_bus_process(bus, NULL);
        /* check if any match changed bus_cb_idx, then call correct callback */
        if (_cb.userdata.bus_mod_idx != MODULES_NUM) {
            /* 
             * Run callbacks before poll_cb, as poll_cb will call
             * started_cb, thus starting all of dependent modules;
             * some of these modules may hook a callback on this module
             * and that callback would be ran if poll_cb was before run_callbacks()
             */
            run_callbacks(_cb.userdata.bus_mod_idx, _cb.userdata.ptr);
            if (_cb.userdata.ptr) {
                free(_cb.userdata.ptr);
                _cb.userdata.ptr = NULL;
            }
            poll_cb(_cb.userdata.bus_mod_idx);
        }
    } while (r > 0);
}

/* 
 * Store systembus ptr in tmp var;
 * set userbus as new bus;
 * call callback() on the new bus;
 * restore systembus;
 */
void bus_callback(void) {
    sd_bus *tmp = bus;
    bus = userbus;
    callback();
    bus = tmp;
}

/*
 * Call a method on bus and store its result of type userptr_type in userptr.
 */
int call(void *userptr, const char *userptr_type, const struct bus_args *a, const char *signature, ...) {
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *m = NULL, *reply = NULL;
    sd_bus *tmp = a->type == USER ? userbus : bus;
    
    int r = sd_bus_message_new_method_call(tmp, &m, a->service, a->path, a->interface, a->member);
    if (check_err(r, &error)) {
        goto finish;
    }

    va_list args;
    va_start(args, signature);
    
#if LIBSYSTEMD_VERSION >= 234
    sd_bus_message_appendv(m, signature, args);
#else
    int i = 0;
    char *s;
    int val;
    while (signature[i] != '\0') {
        switch (signature[i]) {
            case SD_BUS_TYPE_STRING:
                s = va_arg(args, char *);
                r = sd_bus_message_append_basic(m, 's', s);
                if (check_err(r, &error)) {
                    goto finish;
                }
                break;
            case SD_BUS_TYPE_INT32:
                val = va_arg(args, int);
                r = sd_bus_message_append_basic(m, 'i', &val);
                if (check_err(r, &error)) {
                    goto finish;
                }
                break;
            default:
                WARN("Wrong signature in bus call: %c.\n", signature[i]);
                break;
        }
        i++;
    }
#endif
    va_end(args);
    r = sd_bus_call(tmp, m, 0, &error, &reply);
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
        } else if (userptr_type[0] == 'a') {
            r = sd_bus_message_enter_container(reply, SD_BUS_TYPE_ARRAY, userptr_type + 1);
            if (r >= 0) {
                int i = 0;
                while (sd_bus_message_read(reply, userptr_type + 1, &(((double *)userptr)[i])) > 0) {
                    i++;
                }
            }
        } else {
            r = sd_bus_message_read(reply, userptr_type, userptr);
        }
        check_err(r, &error);
    }

finish:
    free_bus_structs(&error, m, reply);
    return r;
}

/*
 * Add a match on bus on certain signal for cb callback
 */
int add_match(const struct bus_args *a, sd_bus_slot **slot, sd_bus_message_handler_t cb) {
    sd_bus *tmp = a->type == USER ? userbus : bus;
    
    char match[500] = {0};
    snprintf(match, sizeof(match), "type='signal', sender='%s', interface='%s', member='%s', path='%s'", a->service, a->interface, a->member, a->path);
    int r = sd_bus_add_match(tmp, slot, match, cb, &_cb.userdata);
    check_err(r, NULL);
    return r;
}

/*
 * Set property of type "type" value to "value". It correctly handles 'u' and 's' types.
 */
int set_property(const struct bus_args *a, const char type, const char *value) {
    sd_bus *tmp = a->type == USER ? userbus : bus;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    int r = 0;

    switch (type) {
        case SD_BUS_TYPE_UINT32:
            r = sd_bus_set_property(tmp, a->service, a->path, a->interface, a->member, &error, "u", atoi(value));
            break;
        case SD_BUS_TYPE_STRING:
            r = sd_bus_set_property(tmp, a->service, a->path, a->interface, a->member, &error, "s", value);
            break;
        default:
            WARN("Wrong signature in bus call: %c.\n", type);
            break;
    }
    check_err(r, &error);
    free_bus_structs(&error, NULL, NULL);
    return r;
}

/*
 * Get a property of type "type" into userptr.
 */
int get_property(const struct bus_args *a, const char *type, void *userptr) {
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *m = NULL;
    sd_bus *tmp = a->type == USER ? userbus : bus;

    int r = sd_bus_get_property(tmp, a->service, a->path, a->interface, a->member, &error, &m, type);
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
    return r;
}

void add_mod_callback(const struct bus_cb cb) {
    struct bus_cb *tmp = realloc(_cb.callbacks, sizeof(struct bus_cb) * (++_cb.num_callbacks));
    if (tmp) {
        _cb.callbacks = tmp;
        _cb.callbacks[_cb.num_callbacks - 1] = cb;
    } else {
        free(_cb.callbacks);
        ERROR("%s\n", strerror(errno));
    }
}

static void run_callbacks(const enum modules mod, const void *payload) {
    for (int i = 0; i < _cb.num_callbacks; i++) {
        if (_cb.callbacks[i].module == mod) {
            _cb.callbacks[i].cb(payload);
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
static int check_err(int r, sd_bus_error *err) {
    if (r < 0) {
        /* Only leave for EHOSTUNREACH if it comes from clightd */
        if (r == -EHOSTUNREACH && err && err->message) {
            if (strstr(err->message, "/org/clightd/backlight")) {
                ERROR("%s\n", err->message);
                goto end; // this line won't be executed as ERROR() macro does a longjmp for error handling and leave
            }
        }
        /* Don't leave for ebusy/eperm/EHOSTUNREACH errors. eperm may mean that a not-active session called a method on clightd */
        if (r == -EBUSY || r == -EPERM || r == -EHOSTUNREACH) {
            WARN("%s\n", err && err->message ? err->message : strerror(-r));
        } else {
            ERROR("%s\n", err && err->message ? err->message : strerror(-r));
        }
    }

end:
    return r < 0;
}

sd_bus **get_user_bus(void) {
    return &userbus;
}

