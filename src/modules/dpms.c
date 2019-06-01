#include "idler.h"
#include <interface.h>

static int on_new_idle(sd_bus_message *m, void *userdata, __attribute__((unused)) sd_bus_error *ret_error);
static void set_dpms(int dpms_state);
static void upower_callback(const void *ptr);
static void inhibit_callback(const void *ptr);
static void interface_timeout_callback(const void *ptr);

static sd_bus_slot *slot;
static char client[PATH_MAX + 1];
static struct dependency dependencies[] = {
    {SOFT, UPOWER},     // Are we on AC or on BATT?
    {HARD, CLIGHTD},     // We need clightd
    {SOFT, INHIBIT},    // We may get inhibited by powersave
    {SOFT, INTERFACE}   // It adds callbacks on INTERFACE
};
static struct self_t self = {
    .num_deps = SIZE(dependencies),
    .deps =  dependencies,
    .functional_module = 1
};

MODULE(DPMS);

static void init(void) {
    struct bus_cb upower_cb = { UPOWER, upower_callback };
    struct bus_cb inhibit_cb = { INHIBIT, inhibit_callback };
    struct bus_cb interface_inhibit_cb = { INTERFACE, inhibit_callback, "inhibit" };
    struct bus_cb interface_to_cb = { INTERFACE, interface_timeout_callback, "dpms_timeout" };

    int r = idle_init(client, slot, conf.dpms_timeout[state.ac_state], on_new_idle);
    
    INIT_MOD(r == 0 ? DONT_POLL : DONT_POLL_W_ERR, &upower_cb, &inhibit_cb, &interface_inhibit_cb, &interface_to_cb);
}

/* Works everywhere except wayland */
static int check(void) {
    return state.wl_display != NULL && strlen(state.wl_display);
}

static int callback(void) {
    /* Skeleton interface */
    return 0;
}

static void destroy(void) {
    if (slot) {
        slot = sd_bus_slot_unref(slot);
    }
    idle_client_stop(client);
    idle_client_destroy(client);
}

static int on_new_idle(sd_bus_message *m,  __attribute__((unused)) void *userdata, __attribute__((unused)) sd_bus_error *ret_error) {   
    int is_dpms;
    sd_bus_message_read(m, "b", &is_dpms);
    if (is_dpms) {
        state.display_state |= DISPLAY_OFF;
        DEBUG("Entering dpms state...\n");
    } else {
        state.display_state &= ~DISPLAY_OFF;
        DEBUG("Leaving dpms state...\n");
    }
    set_dpms(is_dpms);
    emit_prop("DisplayState");
    return 0;
}

static void set_dpms(int dpms_state) {
    SYSBUS_ARG(args, CLIGHTD_SERVICE, "/org/clightd/clightd/Dpms", "org.clightd.clightd.Dpms", "Set");
    call(NULL, NULL, &args, "ssi", state.display, state.xauthority, dpms_state);
}

static void upower_callback(const void *ptr) {
    int old_ac_state = *(int *)ptr;
    /* Force check that we received an ac_state changed event for real */
    if (old_ac_state != state.ac_state) {
        idle_set_timeout(client, conf.dpms_timeout[state.ac_state]);
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
            idle_client_start(client);
        } else {
            idle_client_stop(client);
        }
    }
}

static void interface_timeout_callback(const void *ptr) {
    idle_set_timeout(client, conf.dpms_timeout[state.ac_state]);
}
