#include "../inc/upower.h"

static void init(void);
static int upower_init(void);
static int on_upower_change(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

static struct dependency dependencies[] = { {HARD, BUS_IX} };
static struct self_t self = {
    .name = "Upower",
    .idx = UPOWER_IX,
    .num_deps = SIZE(dependencies),
    .deps =  dependencies
};

void set_upower_self(void) {
    modules[self.idx].self = &self;
    modules[self.idx].init = init;
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

static int upower_init(void) {
    struct bus_args args = {"org.freedesktop.UPower", "/org/freedesktop/UPower", "org.freedesktop.DBus.Properties", "PropertiesChanged"};
    add_match(&args, on_upower_change);
    
    if (state.quit) {
        state.quit = 0; // do not leave
        return -1;   // disable this module
    }
    /* check initial AC state */
    return on_upower_change(NULL, NULL, NULL);
}

/* 
 *Callback on upower changes: recheck on_battery boolean value 
 */
static int on_upower_change(__attribute__((unused)) sd_bus_message *m, __attribute__((unused)) void *userdata, __attribute__((unused)) sd_bus_error *ret_error) {
    struct bus_args power_args = {"org.freedesktop.UPower",  "/org/freedesktop/UPower", "org.freedesktop.UPower", "OnBattery"};
    
    int on_battery = state.ac_state;
    get_property(&power_args, "b", &state.ac_state);
    if (state.ac_state != on_battery && modules[CAPTURE_IX].inited) {
        INFO(on_battery ? "Ac cable disconnected. Enabling powersaving mode.\n" : "Ac cable connected. Disabling powersaving mode.\n");
        reset_timer(main_p[CAPTURE_IX].fd, conf.timeout[on_battery][state.time]);
    }
    return 0;
}
