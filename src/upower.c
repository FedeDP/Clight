#include "../inc/upower.h"

static void init(void);
static int check(void);
static void callback(void);
static void destroy(void);
static int upower_init(void);
static int on_upower_change(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

static sd_bus_slot *slot;
static struct dependency dependencies[] = { {HARD, BUS} };
static struct self_t self = {
    .name = "Upower",
    .idx = UPOWER,
    .num_deps = SIZE(dependencies),
    .deps =  dependencies
};

void set_upower_self(void) {
    SET_SELF();
}

/*
 * init location:
 * init geoclue and set a match on bus on new location signal
 */
static void init(void) {
    int r = upower_init();
    /* In case of errors, upower_init returns -1 -> disable upower. */
    init_module(r == 0 ? DONT_POLL : DONT_POLL_W_ERR, self.idx);
}

static int check(void) {
    return conf.single_capture_mode;
}

static void callback(void) {
    // Skeleton interface
}

static void destroy(void) {
    /* Destroy this match slot */
    if (slot) {
        sd_bus_slot_unref(slot);
    }
}

// FIXME: we would get signal from battery discharging too i guess
static int upower_init(void) {
    struct bus_args args = {"org.freedesktop.UPower", "/org/freedesktop/UPower/devices", "org.freedesktop.DBus.Properties", "PropertiesChanged"};
    int r = add_match(&args, &slot, on_upower_change);
    if (r < 0) {
        return -1;   // disable this module
    }
    /* check initial AC state */
    return on_upower_change(NULL, NULL, NULL);
}

/* 
 * Callback on upower changes: recheck on_battery boolean value
 */
static int on_upower_change(__attribute__((unused)) sd_bus_message *m, __attribute__((unused)) void *userdata, __attribute__((unused)) sd_bus_error *ret_error) {
    state.bus_cb_idx = self.idx;
    
    struct bus_args power_args = {"org.freedesktop.UPower",  "/org/freedesktop/UPower", "org.freedesktop.UPower", "OnBattery"};
    
    get_property(&power_args, "b", &state.ac_state);
    INFO(state.ac_state ? "Ac cable is disconnected. Powersaving mode enabled.\n" : "Ac cable is connected. Powersaving mode disabled.\n");
    return 0;
}
