#include "bus.h"

#define GET_BUS(a)  sd_bus *tmp = a->bus; if (!tmp) { tmp = a->type == USER_BUS ? userbus : sysbus; } if (!tmp) { return -1; }

static void free_bus_structs(sd_bus_error *err, sd_bus_message *m, sd_bus_message *reply);
static int check_err(int *r, sd_bus_error *err, const char *caller);

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
    
    sd_bus_process(sysbus, NULL);
    sd_bus_process(userbus, NULL);
    
    int bus_fd = sd_bus_get_fd(sysbus);
    int userbus_fd = sd_bus_get_fd(userbus);

    m_register_fd(dup(bus_fd), true, sysbus);
    m_register_fd(dup(userbus_fd), true, userbus);
}

static bool check(void) {
    return true;
}

static bool evaluate(void) {
    return true;
}

static void destroy(void) {
    if (sysbus) {
        sysbus = sd_bus_flush_close_unref(sysbus);
    }
}

static void receive(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
    case FD_UPD: {
        sd_bus *b = (sd_bus *)msg->fd_msg->userptr;
        int r;
        do {
            r = sd_bus_process(b, NULL);
        } while (r > 0);
        if (r == -ENOTCONN || r == -ECONNRESET) {
            modules_quit(r);
        }
        break;
    }
    default:
        break;
    }
}

/*
 * Call a method on bus and store its result of type userptr_type in userptr.
 */
int call(const bus_args *a, const char *signature, ...) {
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *m = NULL, *reply = NULL;
    GET_BUS(a);
    
    int r = sd_bus_message_new_method_call(tmp, &m, a->service, a->path, a->interface, a->member);
    if (check_err(&r, &error, a->caller)) {
        goto finish;
    }
    
    r = sd_bus_message_set_expect_reply(m, a->reply_cb != NULL);
    if (check_err(&r, &error, a->caller)) {
        goto finish;
    }
    
    if (signature && strlen(signature)) {
        va_list args;
        va_start(args, signature);
        sd_bus_message_appendv(m, signature, args);
        va_end(args);
    }
    
    /* Check if we need to wait for a response message */
    if (a->reply_cb != NULL) {
        r = sd_bus_call(tmp, m, 0, &error, &reply);
        if (check_err(&r, &error, a->caller)) {
            goto finish;
        }
        r = a->reply_cb(reply, a->member, a->reply_userdata);
    } else {
        r = sd_bus_send(tmp, m, NULL);
    }
    check_err(&r, &error, a->caller);
    
finish:
    free_bus_structs(&error, m, reply);
    return r;
}

/*
 * Add a match on bus on certain signal for cb callback
 */
int add_match(const bus_args *a, sd_bus_slot **slot, sd_bus_message_handler_t cb) {
    GET_BUS(a);

    int r = sd_bus_match_signal(tmp, slot, a->service, a->path, a->interface, a->member, cb, NULL);
    return check_err(&r, NULL, a->caller);
}

int set_property(const bus_args *a, const char *type, const uintptr_t value) {
    GET_BUS(a);
    sd_bus_error error = SD_BUS_ERROR_NULL;
   
    int r = -EINVAL;
    if (type) {
        r = sd_bus_set_property(tmp, a->service, a->path, a->interface, a->member, &error, type, value);
    }
    check_err(&r, &error, a->caller);
    free_bus_structs(&error, NULL, NULL);
    return r;
}

int get_property(const bus_args *a, const char *type, void *userptr) {
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *m = NULL;
    GET_BUS(a);
    
    int r = -EINVAL;
    if (type) {
        switch (*type) {
        case SD_BUS_TYPE_STRING:
        case SD_BUS_TYPE_OBJECT_PATH: {
            r = sd_bus_get_property(tmp, a->service, a->path, a->interface, a->member, &error, &m, type);
            const char *obj = NULL;
            r = sd_bus_message_read(m, type, &obj);
            if (r >= 0) {
                *((char **)userptr) = strdup(obj); // must be freed by caller
            }
            break;
        }
        default:
            r = sd_bus_get_property_trivial(tmp, a->service, a->path, a->interface, a->member, &error, *type, userptr);
            break;
        }
    }    
    check_err(&r, NULL, a->caller);    
    free_bus_structs(&error, m, NULL);    
    return r;
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

static int check_err(int *r, sd_bus_error *err, const char *caller) {
    if (*r < 0) {
        DEBUG("%s(): %s\n", caller, err && err->message ? err->message : strerror(-*r));
    }
    
    /* -1 on error, 0 ok */
    *r = -(*r < 0);
    return *r;
}

sd_bus *get_user_bus(void) {
    return userbus;
}
