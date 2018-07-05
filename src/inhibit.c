#include "../inc/inhibit.h"
#include "../inc/bus.h"

static void init(void);
static int check(void);
static void callback(void);
static void destroy(void);
static int inhibit_init(void);
static int on_inhibit_change(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

static sd_bus_slot *slot;
static struct dependency dependencies[] = { {HARD, USERBUS}, {HARD, XORG} };
static struct self_t self = {
    .name = "Inhibit",
    .idx = INHIBIT,
    .num_deps = SIZE(dependencies),
    .deps =  dependencies
};

void set_inhibit_self(void) {
    SET_SELF();
}

static void init(void) {
    int r = inhibit_init();
    /* In case of errors, upower_init returns -1 -> disable inhibit. */
    INIT_MOD(r == 0 ? DONT_POLL : DONT_POLL_W_ERR);
}

static int check(void) {
    /* check initial inhibit state */
    struct bus_args inhibit_args = {"org.freedesktop.PowerManagement.Inhibit", "/org/freedesktop/PowerManagement/Inhibit", "org.freedesktop.PowerManagement.Inhibit", "HasInhibit", USER};
    int r = call(&state.pm_inhibited, "b", &inhibit_args, NULL);
    if (r < 0) {
        WARN("PowerManagement inhibition appears to be unsupported.\n");
    }
    return -(r < 0);
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

static int inhibit_init(void) {
    struct bus_args args = {"org.freedesktop.PowerManagement", "/org/freedesktop/PowerManagement/Inhibit", "org.freedesktop.PowerManagement.Inhibit", "HasInhibitChanged", USER};
    return -(add_match(&args, &slot, on_inhibit_change) < 0);
}

/* 
 * Callback on inhibit state changed: recheck new HasInhibit value
 */
static int on_inhibit_change(__attribute__((unused)) sd_bus_message *m, void *userdata, __attribute__((unused)) sd_bus_error *ret_error) {
    struct bus_match_data *data = (struct bus_match_data *) userdata;
    data->bus_mod_idx = self.idx;
    /* Fill data->ptr with old inhibit state */
    data->ptr = malloc(sizeof(int));
    *(int *)(data->ptr) = state.pm_inhibited;

    struct bus_args args = {"org.freedesktop.PowerManagement.Inhibit", "/org/freedesktop/PowerManagement/Inhibit", "org.freedesktop.PowerManagement.Inhibit", "HasInhibit", USER};
    call(&state.pm_inhibited, "b", &args, NULL);
    
    INFO("PowerManagement inhibition %s.\n", state.pm_inhibited ? "enabled" : "disabled");
    return 0;
}

