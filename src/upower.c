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
}

static int upower_init(void) {
    struct bus_args args = {
        .service = "org.freedesktop.UPower",
        .interface = "org.freedesktop.DBus.Properties",
        .member = "PropertiesChanged",
        .path = "/org/freedesktop/UPower"
    };
    add_match(&args, on_upower_change);
    int ret = 0;
    if (state.quit) {
        state.quit = 0; // do not leave
        ret = -1;   // disable this module
    } else {
        /* check initial AC state */
        on_upower_change(NULL, NULL, NULL);
    }
    return ret;
}

/* Callback on upower changes: recheck on_battery boolean value */
static int on_upower_change(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    struct bus_args power_args = {"org.freedesktop.UPower",  "/org/freedesktop/UPower", "org.freedesktop.UPower", "OnBattery"};
    
    int on_battery = -1;
    get_property(&power_args, "b", &on_battery);
    INFO(on_battery ? "Ac cable disconnected. Enabling powersaving mode.\n" : "Ac cable connected. Disabling powersaving mode.\n");
    return 0;
}
