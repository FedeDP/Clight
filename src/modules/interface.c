#include <module/map.h>
#include "interface.h"
#include "my_math.h"
#include "config.h"

#define CLIGHT_COOKIE -1
#define CLIGHT_INH_KEY "LockClight"

typedef struct {
    int cookie;
    int refs;
    const char *app;
    const char *reason;
} lock_t;

/** org.freedesktop.ScreenSaver spec implementation **/
static void lock_dtor(void *data);
static int start_inhibit_monitor(void);
static void inhibit_parse_msg(sd_bus_message *m);
static int on_bus_name_changed(sd_bus_message *m, UNUSED void *userdata, UNUSED sd_bus_error *ret_error);
static int create_inhibit(int *cookie, const char *key, const char *app_name, const char *reason);
static int drop_inhibit(int *cookie, const char *key, bool force);
static int method_clight_inhibit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_clight_changebl(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_inhibit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_uninhibit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_simulate_activity(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_get_inhibit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

/** Clight bus api **/
static int method_capture(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_load(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_unload(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_pause(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_store_conf(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

static const char object_path[] = "/org/clight/clight";
static const char bus_interface[] = "org.clight.clight";
static const char sc_interface[] = "org.freedesktop.ScreenSaver";

/* Names should match _UPD topic names here as a signal is emitted on each topic */
static const sd_bus_vtable clight_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("Version", "s", NULL, offsetof(state_t, version), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("ClightdVersion", "s", NULL, offsetof(state_t, clightd_version), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Sunrise", "t", NULL, offsetof(state_t, day_events[SUNRISE]), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Sunset", "t", NULL, offsetof(state_t, day_events[SUNSET]), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("NextEvent", "i", NULL, offsetof(state_t, next_event), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("DayTime", "i", NULL, offsetof(state_t, day_time), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("InEvent", "b", NULL, offsetof(state_t, in_event), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("DisplayState", "i", NULL, offsetof(state_t, display_state), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("AcState", "i", NULL, offsetof(state_t, ac_state), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("LidState", "i", NULL, offsetof(state_t, lid_state), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Inhibited", "b", NULL, offsetof(state_t, inhibited), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("PmInhibited", "b", NULL, offsetof(state_t, pm_inhibited), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("SensorAvail", "b", NULL, offsetof(state_t, sens_avail), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("BlPct", "d", NULL, offsetof(state_t, current_bl_pct), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("KbdPct", "d", NULL, offsetof(state_t, current_kbd_pct), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("AmbientBr", "d", NULL, offsetof(state_t, ambient_br), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Temp", "i", NULL, offsetof(state_t, current_temp), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Location", "(dd)", get_location, offsetof(state_t, current_loc), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("ScreenComp", "d", NULL, offsetof(state_t, screen_comp), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Suspended", "b", NULL, offsetof(state_t, suspended), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_METHOD("Capture", "bb", NULL, method_capture, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Inhibit", "b", NULL, method_clight_inhibit, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("IncBl", "d", NULL, method_clight_changebl, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("DecBl", "d", NULL, method_clight_changebl, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Load", "s", NULL, method_load, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Unload", "s", NULL, method_unload, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Pause", "b", NULL, method_pause, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

static const sd_bus_vtable conf_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_WRITABLE_PROPERTY("Verbose", "b", NULL, NULL, offsetof(conf_t, verbose), 0),
    SD_BUS_WRITABLE_PROPERTY("ResumeDelay", "i", NULL, NULL, offsetof(conf_t, resumedelay), 0),
    SD_BUS_METHOD("Store", NULL, NULL, method_store_conf, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

static const sd_bus_vtable sc_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Inhibit", "ss", "u", method_inhibit, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("UnInhibit", "u", NULL, method_uninhibit, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("SimulateUserActivity", NULL, NULL, method_simulate_activity, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD_WITH_OFFSET("GetActive", NULL, "b", method_get_inhibit, offsetof(state_t, inhibited), SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

DECLARE_MSG(bl_to_req, BL_TO_REQ);
DECLARE_MSG(bl_req, BL_REQ);
DECLARE_MSG(kbd_to_req, KBD_TO_REQ);
DECLARE_MSG(dimmer_to_req, DIMMER_TO_REQ);
DECLARE_MSG(dpms_to_req, DPMS_TO_REQ);
DECLARE_MSG(scr_to_req, SCR_TO_REQ);
DECLARE_MSG(capture_req, CAPTURE_REQ);
DECLARE_MSG(curve_req, CURVE_REQ);
DECLARE_MSG(kbd_curve_req, KBD_CURVE_REQ);
DECLARE_MSG(loc_req, LOCATION_REQ);
DECLARE_MSG(simulate_req, SIMULATE_REQ);
DECLARE_MSG(suspend_req, SUSPEND_REQ);

static map_t *lock_map;
static sd_bus *userbus, *monbus;
static sd_bus_message *bl_curve_message; // this is used to keep backlight curve points data lingering around in set_curve
static sd_bus_message *kbd_curve_message; // this is used to keep kbd backlight curve points data lingering around in set_curve
static sd_bus_slot *lock_slot;

MODULE("INTERFACE");

static void init(void) {
    const char conf_path[] = "/org/clight/clight/Conf";
    const char sc_path_full[] = "/org/freedesktop/ScreenSaver";
    const char sc_path[] = "/ScreenSaver";
    const char conf_interface[] = "org.clight.clight.Conf";
   
    userbus = get_user_bus();
    
    /* Main State interface */
    int r = sd_bus_add_object_vtable(userbus,
                                NULL,
                                object_path,
                                bus_interface,
                                clight_vtable,
                                &state);

    /* Generic Conf interface */
    r += sd_bus_add_object_vtable(userbus,
                                NULL,
                                conf_path,
                                conf_interface,
                                conf_vtable,
                                &conf);
    
    if (!conf.inh_conf.disabled) {
            /*
            * ScreenSaver implementation:
            * take both /ScreenSaver and /org/freedesktop/ScreenSaver paths
            * as they're both used by applications.
            * Eg: chromium/libreoffice use full path, while vlc uses /ScreenSaver
            *
            * Avoid checking for errors!!
            */
            sd_bus_add_object_vtable(userbus,
                                        NULL,
                                        sc_path,
                                        sc_interface,
                                        sc_vtable,
                                        &state);

            sd_bus_add_object_vtable(userbus,
                                    NULL,
                                    sc_path_full,
                                    sc_interface,
                                    sc_vtable,
                                    &state);
    }

    if (r < 0) {
        WARN("Could not create %s dbus interface: %s\n", bus_interface, strerror(-r));
    } else {
        r = sd_bus_request_name(userbus, bus_interface, 0);
        if (r < 0) {
            WARN("Failed to create %s dbus interface: %s\n", bus_interface, strerror(-r));
        } else {
            /* Subscribe to any topic expept REQUESTS */
            m_subscribe("^[^Req].*");
            
            /** org.freedesktop.ScreenSaver API **/
            if (!conf.inh_conf.disabled) {
                if (sd_bus_request_name(userbus, sc_interface, SD_BUS_NAME_REPLACE_EXISTING) < 0) {
                    // Debug as this is not a warning, it happens very often
                    DEBUG("Failed to create %s dbus interface: %s\n", sc_interface, strerror(-r));
                    INFO("Monitoring requests to %s name owner.\n", sc_interface);
                    if (start_inhibit_monitor() != 0) {
                        WARN("Failed to register %s inhibition monitor.\n", sc_interface);
                    }
                } else {
                    INFO("%s dbus interface exposed.\n", sc_interface);
                }
                lock_map = map_new(true, lock_dtor);
            }
            /**                                 **/
        }
    }
    
    if (r < 0) {
        WARN("Failed to init. Killing module.\n");
        module_deregister((self_t **)&self());
    }
}

static bool check(void) {
    return true;
}

static bool evaluate() {
    return !conf.wizard;
}

static void receive(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
    case FD_UPD: {
        sd_bus *b = (sd_bus *)msg->fd_msg->userptr;
        int r;
        do {
            sd_bus_message *m = NULL;
            r = sd_bus_process(b, &m);
            if (m) {
                inhibit_parse_msg(m);
                sd_bus_message_unref(m);
            }
        } while (r > 0);
        if (r < 0) {
            // in case of error, kill the monitor bus and open a new one
            WARN("%s\n", strerror(-r));
            m_deregister_fd(msg->fd_msg->fd);
            monbus = sd_bus_flush_close_unref(monbus);
            start_inhibit_monitor();
        }
        break;
    }
    case SYSTEM_UPD:
        // We just do not want to process it in default case
        break;
    default:
        if (userbus) {
            DEBUG("Emitting '%s' property\n", msg->ps_msg->topic);
            sd_bus_emit_properties_changed(userbus, object_path, bus_interface, msg->ps_msg->topic, NULL);
        }
        break;
    }
}

static void destroy(void) {
    if (userbus) {
        sd_bus_release_name(userbus, bus_interface);
        if (!conf.inh_conf.disabled) {
            sd_bus_release_name(userbus, sc_interface);
        }
        userbus = sd_bus_flush_close_unref(userbus);
    }
    if (monbus) {
        monbus = sd_bus_flush_close_unref(monbus);
    }
    map_free(lock_map);
    bl_curve_message = sd_bus_message_unref(bl_curve_message);
    kbd_curve_message = sd_bus_message_unref(kbd_curve_message);
}

static void lock_dtor(void *data) {
    lock_t *l = (lock_t *)data;
    free((void *)l->app);
    free((void *)l->reason);
    free(l);
}

/** org.freedesktop.ScreenSaver spec implementation: https://people.freedesktop.org/~hadess/idle-inhibition-spec/re01.html **/

/* 
 * Fallback to monitoring org.freedesktop.ScreenSaver bus name to receive Inhibit/UnhInhibit notifications
 * when org.freedesktop.ScreenSaver name could not be owned by Clight (ie: there is some other app that is owning it).
 * 
 * Stolen from: https://github.com/systemd/systemd/blob/master/src/busctl/busctl.c#L1203 (busctl monitor)
 */
static int start_inhibit_monitor(void) {
    int r = sd_bus_new(&monbus);
    if (r < 0) {
        WARN("Failed to create monitor: %m\n");
        return r;
    }
    
    r = sd_bus_set_monitor(monbus, true);
    if (r < 0) {
        WARN("Failed to set monitor mode: %m\n");
        return r;
    }
    
    r = sd_bus_negotiate_creds(monbus, true, _SD_BUS_CREDS_ALL);
    if (r < 0) {
        WARN("Failed to enable credentials: %m\n");
        return r;
    }
    
    r = sd_bus_negotiate_timestamp(monbus, true);
    if (r < 0) {
        WARN("Failed to enable timestamps: %m\n");
        return r;
    }
    
    r = sd_bus_negotiate_fds(monbus, true);
    if (r < 0) {
        WARN("Failed to enable fds: %m\n");
        return r;
    }
    
    r = sd_bus_set_bus_client(monbus, true);
    if (r < 0) {
        WARN("Failed to set bus client: %m\n");
        return r;
    }
    
    /* Set --user address */
    const char *addr = NULL;
    sd_bus_get_address(userbus, &addr);
    sd_bus_set_address(monbus, addr);
    
    sd_bus_start(monbus);
    
    USERBUS_ARG(args, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus.Monitoring", "BecomeMonitor");
    args.bus = monbus;
    
    r = call(&args, "asu", 1, "destination='org.freedesktop.ScreenSaver'", 0);
    if (r == 0) {
        sd_bus_process(monbus, NULL);
        int fd = sd_bus_get_fd(monbus);
        m_register_fd(dup(fd), true, monbus);
    }
    return r;
}

static void inhibit_parse_msg(sd_bus_message *m) {
    if (sd_bus_message_get_member(m)) {
        const char *member = sd_bus_message_get_member(m);
        const char *signature = sd_bus_message_get_signature(m, false);
        const char *interface = sd_bus_message_get_interface(m);
        
        for (int i = 1; i <= 2; i++) {
            if (!strcmp(interface, sc_interface)
                && !strcmp(member, sc_vtable[i].x.method.member)
                && !strcmp(signature, sc_vtable[i].x.method.signature)) {
                
                switch (i) {
                case 1: { // Inhibit!!
                    int cookie = 0;
                    char *app_name = NULL, *reason = NULL;
                    int r = sd_bus_message_read(m, "ss", &app_name, &reason); 
                    if (r < 0) {
                        WARN("Failed to parse parameters: %s\n", strerror(-r));
                    } else {
                        create_inhibit(&cookie, sd_bus_message_get_sender(m), app_name, reason);
                    }
                    break;
                }
                case 2: // UnInhibit!!
                    drop_inhibit(NULL, sd_bus_message_get_sender(m), false);
                    break;
                default:
                    break;
                }
            }
        }
    }
}

/*
 * org.freedesktop.ScreenSaver spec:
 * Inhibition will stop when the UnInhibit function is called, 
 * or the application disconnects from the D-Bus session bus (which usually happens upon exit).
 * 
 * Polling on NameOwnerChanged dbus signals.
 */
static int on_bus_name_changed(sd_bus_message *m, UNUSED void *userdata, UNUSED sd_bus_error *ret_error) {
    const char *name = NULL, *old_owner = NULL, *new_owner = NULL;
    if (sd_bus_message_read(m, "sss", &name, &old_owner, &new_owner) >= 0) {
        if (map_has_key(lock_map, old_owner) && (!new_owner || !strlen(new_owner))) {
            drop_inhibit(NULL, old_owner, true);
        }
    }
    return 0;
}

static void publish_inhibition(const bool new, const bool force) {
    /* 
     * Declare msg on heap as sometimes (STEAM i am looking at you!)
     * we receive inhibition req too fast,
     * overriding values from currently sent inhibition 
     * before it gets received by INHIBIT module
     */
    DECLARE_HEAP_MSG(inhibit_req, INHIBIT_REQ);
    inhibit_req->inhibit.old = state.inhibited;
    inhibit_req->inhibit.new = new;
    inhibit_req->inhibit.force = force;
    M_PUB(inhibit_req);
}

static int create_inhibit(int *cookie, const char *key, const char *app_name, const char *reason) {
    lock_t *l = map_get(lock_map, key);
    if (l) {
        l->refs++;
        *cookie = l->cookie;
    } else {
        lock_t *l = malloc(sizeof(lock_t));
        if (l) {
            if (*cookie != CLIGHT_COOKIE) {
                *cookie = random();
            }
            l->cookie = *cookie;
            l->refs = 1;
            l->app = strdup(app_name);
            l->reason = strdup(reason);
            
            DEBUG("New ScreenSaver inhibition held by '%s': '%s'. Cookie: %d.\n", l->app, l->reason, l->cookie);
            publish_inhibition(true, false);
            map_put(lock_map, key, l);
            
            if (map_length(lock_map) == 1) {
                /* Start listening on NameOwnerChanged signals */
                USERBUS_ARG(args, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "NameOwnerChanged");
                add_match(&args, &lock_slot, on_bus_name_changed);
            }
        } else {
            return -1;
        }
    }
    return 0;
}

static int drop_inhibit(int *cookie, const char *key, bool force) {
    lock_t *l = map_get(lock_map, key);
    if (!l && cookie) {
        /* May be another sender is asking to drop a cookie? Linear search */
        for (map_itr_t *itr = map_itr_new(lock_map); itr; itr = map_itr_next(itr)) {
            lock_t *tmp = (lock_t *)map_itr_get_data(itr);
            if (tmp->cookie == *cookie) {
                l = tmp;
                free(itr);
                itr = NULL;
            }
        }
    }

    if (l) {
        if (!force) {
            l->refs--;
        } else {
            l->refs = 0;
        }
        if (l->refs == 0) {
            DEBUG("Dropped ScreenSaver inhibition held by '%s': '%s'. Cookie: %d.\n", l->app, l->reason, l->cookie);
            publish_inhibition(false, !strcmp(key, CLIGHT_INH_KEY)); // forcefully disable inhibition for Clight INTERFACE Inhibit "false"
            map_remove(lock_map, key);
            
            if (map_length(lock_map) == 0) {
                /* Stop listening on NameOwnerChanged signals */
                lock_slot = sd_bus_slot_unref(lock_slot);
            }
        }
        return 0;
    }
    return -1;
}

static int method_clight_inhibit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int inhibit;
    VALIDATE_PARAMS(m, "b", &inhibit);
    
    if (!conf.inh_conf.disabled) {
        int ret = 0;
        if (inhibit) {
            int cookie = CLIGHT_COOKIE;
            ret = create_inhibit(&cookie, CLIGHT_INH_KEY, "Clight", "user requested");
        } else {
            ret = drop_inhibit(NULL, CLIGHT_INH_KEY, true);
        }

        if (ret == 0) {
            return sd_bus_reply_method_return(m, NULL);
        }
    } else {
        WARN("Inhibit module is disabled.\n");
    }
    sd_bus_error_set_errno(ret_error, EINVAL);
    return -EINVAL;
}

static int method_clight_changebl(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    double change_pct;
    VALIDATE_PARAMS(m, "d", &change_pct);
    
    if (change_pct > 0.0 && change_pct <= 1.0) {
        bl_req.bl.smooth = -1;
        
        if (!strcmp(sd_bus_message_get_member(m), "IncBl")) {
            bl_req.bl.new = state.current_bl_pct + change_pct;
            if (bl_req.bl.new > 1.0) {
                bl_req.bl.new = 1.0;
            }
        } else {
            bl_req.bl.new = state.current_bl_pct - change_pct;
            if (bl_req.bl.new < 0.0) {
                bl_req.bl.new = 0.0;
            }
        }
        M_PUB(&bl_req);
        return sd_bus_reply_method_return(m, NULL);
    }
    sd_bus_error_set_errno(ret_error, EINVAL);
    return -EINVAL;
}

static int method_inhibit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    char *app_name = NULL, *reason = NULL;
    
    VALIDATE_PARAMS(m, "ss", &app_name, &reason);
    
    int cookie = 0;
    if (create_inhibit(&cookie, sd_bus_message_get_sender(m), app_name, reason) == 0) {
        return sd_bus_reply_method_return(m, "u", cookie);
    }
    sd_bus_error_set_errno(ret_error, ENOMEM);
    return -ENOMEM;
}

static int method_uninhibit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int cookie;
    
    VALIDATE_PARAMS(m, "u", &cookie);
    
    if (drop_inhibit(&cookie, sd_bus_message_get_sender(m), false) == 0) {
        return sd_bus_reply_method_return(m, NULL);
    }
    sd_bus_error_set_errno(ret_error, EINVAL);
    return -EINVAL;
}

static int method_simulate_activity(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    M_PUB(&simulate_req);
    return sd_bus_reply_method_return(m, NULL);
}

static int method_get_inhibit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    return sd_bus_reply_method_return(m, "b", *(bool *)userdata);
}

/** Clight bus api **/
                       
static int method_capture(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int reset_timer, capture_only;
    VALIDATE_PARAMS(m, "bb", &reset_timer, &capture_only);
    
    capture_req.capture.reset_timer = reset_timer;
    capture_req.capture.capture_only = capture_only;
    M_PUB(&capture_req);
    return sd_bus_reply_method_return(m, NULL);
}

static int method_load(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    const char *module_path;

    VALIDATE_PARAMS(m, "s", &module_path);
    if (m_load(module_path) == MOD_OK) {
        INFO("'%s' loaded.\n", module_path);
        return sd_bus_reply_method_return(m, NULL);
    }

    WARN("'%s' failed to load.\n", module_path);
    sd_bus_error_set_errno(ret_error, EINVAL);
    return -EINVAL;
}

static int method_unload(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    const char *module_path;

    VALIDATE_PARAMS(m, "s", &module_path);
    if (m_unload(module_path) == MOD_OK) {
        INFO("'%s' unloaded.\n", module_path);
        return sd_bus_reply_method_return(m, NULL);
    }

    WARN("'%s' failed to unload.\n", module_path);
    sd_bus_error_set_errno(ret_error, EINVAL);
    return -EINVAL;
}

static int method_pause(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int paused;
    VALIDATE_PARAMS(m, "b", &paused);
    suspend_req.suspend.new = paused;
    M_PUB(&suspend_req);
    return sd_bus_reply_method_return(m, NULL);
}

int get_curve(sd_bus *bus, const char *path, const char *interface, const char *property,
                     sd_bus_message *reply, void *userdata, sd_bus_error *error) {
    
    curve_t *c = (curve_t *)userdata;
    return sd_bus_message_append_array(reply, 'd', c->points, c->num_points * sizeof(double));
}

int set_curve(sd_bus *bus, const char *path, const char *interface, const char *property,
                     sd_bus_message *value, void *userdata, sd_bus_error *error) {

    sd_bus_message **curve_msg = NULL;
    message_t *msg = NULL;
    curve_t *curve = NULL;
    
    if (userdata == &conf.sens_conf.default_curve[ON_AC] || userdata == &conf.sens_conf.default_curve[ON_BATTERY]) {
        curve_msg = &bl_curve_message;
        msg = &curve_req;
        curve = conf.sens_conf.default_curve;
    } else {
        curve_msg = &kbd_curve_message;
        msg = &kbd_curve_req;
        curve = conf.kbd_conf.curve;
    }
    
    /* Unref last curve message, if any */
    sd_bus_message_unrefp(curve_msg);

    double *data = NULL;
    size_t length;
    int r = sd_bus_message_read_array(value, 'd', (const void**) &data, &length);
    if (r < 0) {
        WARN("Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    msg->curve.num_points = length / sizeof(double);
    if (msg->curve.num_points > MAX_SIZE_POINTS) {
        WARN("Wrong parameters.\n");
        sd_bus_error_set_const(error, SD_BUS_ERROR_FAILED, "Wrong parameters.");
        r = -EINVAL;
    } else {
        msg->curve.state = ON_AC;
        if (userdata == &curve[ON_BATTERY]) {
            msg->curve.state = ON_BATTERY;
        }
        msg->curve.regression_points = data;
        *curve_msg = sd_bus_message_ref(value);
        M_PUB(msg);
    }
    return r;
}

int get_location(sd_bus *bus, const char *path, const char *interface, const char *property,
                        sd_bus_message *reply, void *userdata, sd_bus_error *error) {
    loc_t *l = (loc_t *)userdata;
    return sd_bus_message_append(reply, "(dd)", l->lat, l->lon);
}

int set_location(sd_bus *bus, const char *path, const char *interface, const char *property,
                        sd_bus_message *value, void *userdata, sd_bus_error *error) {

    VALIDATE_PARAMS(value, "(dd)", &loc_req.loc.new.lat, &loc_req.loc.new.lon);

    DEBUG("New location from BUS api: %.2lf %.2lf\n", loc_req.loc.new.lat, loc_req.loc.new.lat);
    M_PUB(&loc_req);
    return r;
}

int set_timeouts(sd_bus *bus, const char *path, const char *interface, const char *property,
                            sd_bus_message *value, void *userdata, sd_bus_error *error) {    
    /* Check if we modified currently used timeout! */
    message_t *msg = NULL;
    if (userdata == &conf.bl_conf.timeout[ON_AC][DAY]) {
        msg = &bl_to_req;
        bl_to_req.to.daytime = DAY;
        bl_to_req.to.state = ON_AC;
    } else if (userdata == &conf.bl_conf.timeout[ON_AC][NIGHT]) {
        msg = &bl_to_req;
        bl_to_req.to.daytime = NIGHT;
        bl_to_req.to.state = ON_AC;
    } else if (userdata == &conf.bl_conf.timeout[ON_AC][IN_EVENT]) {
        msg = &bl_to_req;
        bl_to_req.to.daytime = IN_EVENT;
        bl_to_req.to.state = ON_AC;
    } else if (userdata == &conf.bl_conf.timeout[ON_BATTERY][DAY]) {
        msg = &bl_to_req;
        bl_to_req.to.daytime = DAY;
        bl_to_req.to.state = ON_BATTERY;
    } else if (userdata == &conf.bl_conf.timeout[ON_BATTERY][NIGHT]) {
        msg = &bl_to_req;
        bl_to_req.to.daytime = NIGHT;
        bl_to_req.to.state = ON_BATTERY;
    } else if (userdata == &conf.bl_conf.timeout[ON_BATTERY][IN_EVENT]) {
        msg = &bl_to_req;
        bl_to_req.to.daytime = IN_EVENT;
        bl_to_req.to.state = ON_BATTERY;
    }  else if (userdata == &conf.kbd_conf.timeout[ON_AC]) {
        msg = &kbd_to_req;
        kbd_to_req.to.state = ON_AC;
    } else if (userdata == &conf.kbd_conf.timeout[ON_BATTERY]) {
        msg = &kbd_to_req;
        kbd_to_req.to.state = ON_BATTERY;
    } else if (userdata == &conf.dim_conf.timeout[ON_AC]) {
        msg = &dimmer_to_req;
        dimmer_to_req.to.state = ON_AC;
    } else if (userdata == &conf.dim_conf.timeout[ON_BATTERY]) {
        msg = &dimmer_to_req;
        dimmer_to_req.to.state = ON_BATTERY;
    } else if (userdata == &conf.dpms_conf.timeout[ON_AC]) {
        msg = &dpms_to_req;
        dpms_to_req.to.state = ON_AC;
    } else if (userdata == &conf.dpms_conf.timeout[ON_BATTERY]) {
        msg = &dpms_to_req;
        dimmer_to_req.to.state = ON_BATTERY;
    } else if (userdata == &conf.screen_conf.timeout[ON_AC]) {
        msg = &scr_to_req;
        scr_to_req.to.state = ON_AC;
    } else if (userdata == &conf.screen_conf.timeout[ON_BATTERY]) {
        msg = &scr_to_req;
        scr_to_req.to.state = ON_BATTERY;
    }
    
    VALIDATE_PARAMS(value, "i", &msg->to.new);

    if (msg) {
        M_PUB(msg);
    }
    return r;
}

static int method_store_conf(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int r = -1;
    if (store_config(LOCAL) == 0) {
        r = sd_bus_reply_method_return(m, NULL);
    } else {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Failed to store conf.");
    }
    return r;
}
