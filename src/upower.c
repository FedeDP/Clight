#include "../inc/upower.h"

static void init(void);
static int check(void);
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
    modules[self.idx].self = &self;
    modules[self.idx].init = init;
    modules[self.idx].check = check;
    modules[self.idx].destroy = destroy;
    set_self_deps(&self);
}

/*
 * init location:
 * init geoclue and set a match on bus on new location signal
 */
static void init(void) {
    int r = upower_init();
    /* In case of errors, upower_init returns -1 -> disable upower. */
    init_module(r == 0 ? DONT_POLL : DONT_POLL_W_ERR, self.idx, NULL);
    poll_cb(self.idx);
}

static int check(void) {
    return conf.single_capture_mode;
}

static void destroy(void) {
    /* Destroy this match slot */
    if (slot) {
        sd_bus_slot_unref(slot);
    }
}

static int upower_init(void) {
    struct bus_args args = {"org.freedesktop.UPower", "/org/freedesktop/UPower", "org.freedesktop.DBus.Properties", "PropertiesChanged"};
    add_match(&args, &slot, on_upower_change);
    if (state.quit) {
        state.quit = 0; // do not leave
        return -1;   // disable this module
    }
    /* check initial AC state */
    return on_upower_change(NULL, NULL, NULL);
}

/* 
 * Callback on upower changes: recheck on_battery boolean value 
 */
static int on_upower_change(__attribute__((unused)) sd_bus_message *m, __attribute__((unused)) void *userdata, __attribute__((unused)) sd_bus_error *ret_error) {
    struct bus_args power_args = {"org.freedesktop.UPower",  "/org/freedesktop/UPower", "org.freedesktop.UPower", "OnBattery"};
    
    int on_battery = state.ac_state;
    get_property(&power_args, "b", &state.ac_state);
    if (state.ac_state != on_battery) {
        INFO(state.ac_state ? "Ac cable disconnected. Enabling powersaving mode.\n" : "Ac cable connected. Disabling powersaving mode.\n");
        // FIXME: reset all modules-dependent-on-upower timers (ie: dimmer too!)
        if (modules[BRIGHTNESS].inited && !state.fast_recapture) {
            if (conf.max_backlight_pct[ON_BATTERY] != conf.max_backlight_pct[ON_AC]) {
                /* if different max values is set, do a capture right now to set correct new brightness value */
                set_timeout(0, 1, main_p[BRIGHTNESS].fd, 0);
            } else {
                reset_timer(main_p[BRIGHTNESS].fd, conf.timeout[on_battery][state.time]);
            }
        }
    }
    return 0;
}
