#include <backlight.h>
#include <bus.h>
#include <interface.h>

static int idle_init(void);
static int idle_get_client(void);
static int idle_hook_update(void);
static int idle_set_timeout(void);
static int idle_set_const_properties(void);
static int idle_client_start(void);
static int idle_client_stop(void);
static int idle_client_destroy(void);
static int on_new_idle(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static void dim_backlight(const double pct);
static void restore_backlight(const double pct);
static void upower_callback(const void *ptr);
static void inhibit_callback(const void * ptr);
static void interface_timeout_callback(const void *ptr);

static sd_bus_slot *slot;
static int running;
static char client[PATH_MAX + 1];
static struct dependency dependencies[] = { 
    {SOFT, UPOWER},     // Are we on AC or BATT?
    {SOFT, BACKLIGHT},  // We need BACKLIGHT as we have to be sure that state.current_br_pct is correctly setted
    {HARD, XORG},       // This module is xorg only
    {SOFT, INHIBIT},    // We may get inhibited by powersave
    {HARD, CLIGHTD},    // We need clightd
    {SOFT, INTERFACE}   // It adds callbacks on INTERFACE
};
static struct self_t self = {
    .num_deps = SIZE(dependencies),
    .deps =  dependencies,
    .functional_module = 1
};

MODULE(DIMMER);

static void init(void) {
    struct bus_cb upower_cb = { UPOWER, upower_callback };
    struct bus_cb inhibit_cb = { INHIBIT, inhibit_callback };
    struct bus_cb interface_inhibit_cb = { INTERFACE, inhibit_callback, "inhibit" };
    struct bus_cb interface_to_cb = { INTERFACE, interface_timeout_callback, "dimmer_timeout" };

    int r = idle_init();

    /* 
     * If dimmer is started and BACKLIGHT module is disabled, or automatic calibration is disabled,
     * we need to ensure to start from a well known backlight level.
     * Force 100% backlight level.
     */
    if (!is_running(BACKLIGHT) || conf.no_auto_calib) {
        set_backlight_level(1.0, 0, 0, 0);
    }
    INIT_MOD(r == 0 ? DONT_POLL : DONT_POLL_W_ERR, &upower_cb, &inhibit_cb, &interface_inhibit_cb, &interface_to_cb);
}

static int check(void) {
    return 0;
}

static void destroy(void) {
    if (slot) {
        slot = sd_bus_slot_unref(slot);
    }
    idle_client_stop();
    idle_client_destroy();
}

static void callback(void) {

}

static int idle_init(void) {
    int r = idle_get_client();
    if (r < 0) {
        goto end;
    }
    r = idle_hook_update();
    if (r < 0) {
        goto end;
    }
    r = idle_set_const_properties();
    if (r < 0) {
        goto end;
    }
    r = idle_set_timeout(); // set timeout automatically calls client start

end:
    if (r < 0) {
        WARN("Clight Idle error.\n");
    }
    return -(r < 0);  // - 1 on error
}

static int idle_get_client(void) {
    SYSBUS_ARG(args, CLIGHTD_SERVICE, "/org/clightd/clightd/Idle", "org.clightd.clightd.Idle", "GetClient");
    return call(client, "o", &args, NULL);
}

static int idle_hook_update(void) {
    SYSBUS_ARG(args, CLIGHTD_SERVICE, client, "org.clightd.clightd.Idle.Client", "Idle");
    return add_match(&args, &slot, on_new_idle);
}

static int idle_set_timeout(void) {
    int r = 0;
    if (conf.dimmer_timeout[state.ac_state] > 0) {
        SYSBUS_ARG(to_args, CLIGHTD_SERVICE, client, "org.clightd.clightd.Idle.Client", "Timeout");
        r = set_property(&to_args, 'u', &conf.dimmer_timeout[state.ac_state]);
        r += idle_client_start();
    } else {
        r = idle_client_stop();
    }
    return r;
}

static int idle_set_const_properties(void) {
    SYSBUS_ARG(display_args, CLIGHTD_SERVICE, client, "org.clightd.clightd.Idle.Client", "Display");
    SYSBUS_ARG(xauth_args, CLIGHTD_SERVICE, client, "org.clightd.clightd.Idle.Client", "AuthCookie");
    int r = set_property(&display_args, 's', state.display);
    r += set_property(&xauth_args, 's', state.xauthority);
    return r;
}

static int idle_client_start(void) {
    if (!running && conf.dimmer_timeout[state.ac_state] > 0 && !state.pm_inhibited) {
        SYSBUS_ARG(args, CLIGHTD_SERVICE, client, "org.clightd.clightd.Idle.Client", "Start");
        running = 1;
        return call(NULL, NULL, &args, NULL);
    }
    return 0;
}

static int idle_client_stop(void) {
    if (running) {
        SYSBUS_ARG(args, CLIGHTD_SERVICE, client, "org.clightd.clightd.Idle.Client", "Stop");
        running = 0;
        return call(NULL, NULL, &args, NULL);
    }
    return 0;
}

static int idle_client_destroy(void) {
    if (strlen(client) > 0) {
        SYSBUS_ARG(args, CLIGHTD_SERVICE, "/org/clightd/clightd/Idle", "org.clightd.clightd.Idle", "DestroyClient");
        return call(NULL, NULL, &args, "o", client);
    }
    return 0;
}

static int on_new_idle(sd_bus_message *m, void *userdata, __attribute__((unused)) sd_bus_error *ret_error) {
    static double old_pct = -1.0;

    sd_bus_message_read(m, "b", &state.is_dimmed);
    if (state.is_dimmed) {
        DEBUG("Entering dimmed state...\n");
        old_pct = state.current_bl_pct;
        dim_backlight(conf.dimmer_pct);
    } else if (old_pct >= 0.0) {
        DEBUG("Leaving dimmed state...\n");
        restore_backlight(old_pct);
    }
    emit_prop("Dimmed");
    return 0;
}

static void dim_backlight(const double pct) {
    /* Don't touch backlight if a lower level is already set */
    if (pct >= state.current_bl_pct) {
        DEBUG("A lower than dimmer_pct backlight level is already set. Avoid changing it.\n");
    } else {
        set_backlight_level(pct, !conf.no_smooth_dimmer, conf.dimmer_trans_step, conf.dimmer_trans_timeout);
    }
}

/* restore previous backlight level */
static void restore_backlight(const double pct) {
    set_backlight_level(pct, !conf.no_smooth_backlight, conf.dimmer_trans_step, conf.dimmer_trans_timeout);
}

/* Reset dimmer timeout */
static void upower_callback(const void *ptr) {
    int old_ac_state = *(int *)ptr;
    /* Force check that we received an ac_state changed event for real */
    if (old_ac_state != state.ac_state) {
        idle_set_timeout();
    }
}

/* 
 * If we're getting inhibited, stop idle client.
 * Else, restart it.
 */
static void inhibit_callback(const void *ptr) {
    int old_pm_state = *(int *)ptr;
    if (!!old_pm_state != !!state.pm_inhibited) {
        DEBUG("%s module being %s.\n", self.name, state.pm_inhibited ? "paused" : "restarted");
        if (!state.pm_inhibited) {
            idle_client_start();
        } else {
            idle_client_stop();
        }
    }
}

/*
 * Interface callback to change timeout
 */
static void interface_timeout_callback(const void *ptr) {
    idle_set_timeout();
}
