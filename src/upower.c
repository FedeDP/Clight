#include "../inc/upower.h"
#include "../inc/bus.h"

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

static void init(void) {
    int r = upower_init();
    /* In case of errors, upower_init returns -1 -> disable upower. */
    INIT_MOD(r == 0 ? DONT_POLL : DONT_POLL_W_ERR);
}

static int check(void) {
    return 0; /* Skeleton function needed for modules interface */
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

static int upower_init(void) {
    /* check initial AC state */
    struct bus_args power_args = {"org.freedesktop.UPower",  "/org/freedesktop/UPower", "org.freedesktop.UPower", "OnBattery"};
    int r = get_property(&power_args, "b", &state.ac_state);
    if (r < 0) {
        WARN("Upower appears to be unsupported.\n");
        return -1;   // disable this module
    }
    
    struct bus_args args = {"org.freedesktop.UPower", "/org/freedesktop/UPower", "org.freedesktop.DBus.Properties", "PropertiesChanged"};
    r = add_match(&args, &slot, on_upower_change);
    return -(r < 0);
}

/* 
 * Callback on upower changes: recheck on_battery boolean value
 */
static int on_upower_change(__attribute__((unused)) sd_bus_message *m, void *userdata, __attribute__((unused)) sd_bus_error *ret_error) {
    struct bus_match_data *data = (struct bus_match_data *) userdata;
    data->bus_mod_idx = self.idx;
    /* Fill data->ptr with old ac state */
    data->ptr = malloc(sizeof(int));
    *(int *)(data->ptr) = state.ac_state;
    
    struct bus_args power_args = {"org.freedesktop.UPower",  "/org/freedesktop/UPower", "org.freedesktop.UPower", "OnBattery"};
    
    /* 
     * Store last ac_state in old struct to be matched against new one
     * as we cannot be sure that a OnBattery changed signal has been really sent:
     * our match will receive these signals:
     * .DaemonVersion                      property  s         "0.99.5"     emits-change
     * .LidIsClosed                        property  b         true         emits-change
     * .LidIsPresent                       property  b         true         emits-change
     * .OnBattery                          property  b         false        emits-change
     */
    get_property(&power_args, "b", &state.ac_state);
    if (*(int *)(data->ptr) != state.ac_state) {
        INFO(state.ac_state ? "Ac cable disconnected. Powersaving mode enabled.\n" : "Ac cable connected. Powersaving mode disabled.\n");
    }
    return 0;
}
