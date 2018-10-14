#include <bus.h>

static int upower_init(void);
static int on_upower_change(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

static sd_bus_slot *slot;
static struct dependency dependencies[] = { {HARD, BUS} };
static struct self_t self = {
    .num_deps = SIZE(dependencies),
    .deps =  dependencies
};

MODULE(UPOWER);

static void init(void) {
    int r = upower_init();
    /* In case of errors, upower_init returns -1 -> disable upower. */
    INIT_MOD(r == 0 ? DONT_POLL : DONT_POLL_W_ERR);
}

static int check(void) {
    /* check initial AC state */
    struct bus_args power_args = {"org.freedesktop.UPower",  "/org/freedesktop/UPower", "org.freedesktop.UPower", "OnBattery"};
    int r = get_property(&power_args, "b", &state.ac_state);
    if (r < 0) {
        WARN("Upower appears to be unsupported.\n");
        return -1;   // disable this module
    }
    return 0;
}

static void callback(void) {
    // Skeleton interface
}

static void destroy(void) {
    /* Destroy this match slot */
    if (slot) {
        slot = sd_bus_slot_unref(slot);
    }
}

static int upower_init(void) {
    struct bus_args args = {"org.freedesktop.UPower", "/org/freedesktop/UPower", "org.freedesktop.DBus.Properties", "PropertiesChanged"};
    return add_match(&args, &slot, on_upower_change);
}

/* 
 * Callback on upower changes: recheck on_battery boolean value
 */
static int on_upower_change(__attribute__((unused)) sd_bus_message *m, void *userdata, __attribute__((unused)) sd_bus_error *ret_error) {
    FILL_MATCH_DATA(state.ac_state);
    
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
    int r = get_property(&power_args, "b", &state.ac_state);
    if (!r && (*(int *)(state.userdata.ptr) != state.ac_state)) {
        INFO(state.ac_state ? "Ac cable disconnected. Powersaving mode enabled.\n" : "Ac cable connected. Powersaving mode disabled.\n");
    }
    return 0;
}
