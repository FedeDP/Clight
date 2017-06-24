#include "../inc/upower.h"

static void init(void);
static int check(void);
static void destroy(void);
static int upower_init(void);
static int on_upower_change(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

static sd_bus_slot *slot;
static int num_callbacks;
static upower_cb *callbacks;
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
}

static int check(void) {
    return conf.single_capture_mode;
}

static void destroy(void) {
    /* Destroy this match slot */
    if (slot) {
        sd_bus_slot_unref(slot);
    }
    if (callbacks) {
        free(callbacks);
    }
}

static int upower_init(void) {
    struct bus_args args = {"org.freedesktop.UPower", "/org/freedesktop/UPower", "org.freedesktop.DBus.Properties", "PropertiesChanged"};
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
    struct bus_args power_args = {"org.freedesktop.UPower",  "/org/freedesktop/UPower", "org.freedesktop.UPower", "OnBattery"};
    
    int on_battery = state.ac_state;
    get_property(&power_args, "b", &state.ac_state);
    if (state.ac_state != on_battery) {
        INFO(state.ac_state ? "Ac cable disconnected. Enabling powersaving mode.\n" : "Ac cable connected. Disabling powersaving mode.\n");
        
        for (int i = 0; i < num_callbacks; i++) {
            callbacks[i](on_battery);
        }
    }
    return 0;
}

/* Hook a callback to upower change event */
void add_upower_module_callback(upower_cb callback) {
    if (modules[self.idx].inited) {
        upower_cb *tmp = realloc(callbacks, sizeof(upower_cb) * (++num_callbacks));
        if (tmp) {
            callbacks = tmp;
            callbacks[num_callbacks - 1] = callback;
        } else {
            if (callbacks) {
                free(callbacks);
            }
            ERROR("%s\n", strerror(errno));
        }
    }
}
