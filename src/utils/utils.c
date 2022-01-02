#include "utils.h"
#include <assert.h>

/*
 * On X, use X display variable;
 * On Wl, use Wl display variable;
 * Otherwise, use NULL device 
 * (ie: we are neither on x or wayland; try drm plugin)
 */
const char *fetch_display() {
    /*
     * Use wayland as first case, 
     * as sometimes on wayland you have $DISPLAY env var too!
     */
    const char *wl_display = getenv("WAYLAND_DISPLAY");
    if (!is_string_empty(wl_display)) {
        return wl_display;
    }
    return getenv("DISPLAY");
}

/*
 * On X, use xauthority
 * On Wl, use xdg_runtime_dir
 * Otherwise, use NULL (unneeded with drm plugin)
 */
const char *fetch_env() {
    const char *xauth = getenv("XAUTHORITY");
    if (!is_string_empty(xauth)) {
        return xauth;
    }
    return getenv("XDG_RUNTIME_DIR");
}

/*
 * Only used by gamma and dpms -> in case our display is empty,
 * ie: no state.display neither state.wl_display are set,
 * clight will react to Clightd {Gamma,Dpms}.Changed signals
 * for any display id. Not great, not bad. 
 * It is a very small usecase though,
 * ie: starting clight on tty and using clightd to change Dpms or Gamma 
 * in another graphical session.
 */
bool own_display(const char *display) {
    const char *my_display = fetch_display();
    if (is_string_empty(my_display)) {
        return true;
    }
    return strcmp(display, my_display) == 0;
}

static inline const char *mod_pause_reason_string(enum mod_pause reason) {
    assert(reason != UNPAUSED);
    
    static const char *reasons[] = {
#define X(name, value) #name,
        X_FIELDS
#undef X
    };
    // this counts number of trailing 0;
    // it is undefined for 0
    const int idx = __builtin_ctz(reason) + 1;
    return reasons[idx];
}

bool mod_check_pause(bool pause, int *paused_state, enum mod_pause reason, const char *modname) {
    int old_paused = *paused_state;
    if (pause) {
        *paused_state |= reason;
        if (old_paused == UNPAUSED) {
            DEBUG("Pausing %s: %s\n", modname, mod_pause_reason_string(reason));
        }
        return old_paused == UNPAUSED;
    }
    *paused_state &= ~reason;
    if (old_paused != UNPAUSED && *paused_state == UNPAUSED) {
        DEBUG("Resuming %s: %s\n", modname, mod_pause_reason_string(reason));
        return true;
    }
    return false;
}

bool is_string_empty(const char *str) {
    return str == NULL || str[0] == '\0';
}

bool check_module_sender(const self_t *self, const char *modname, const self_t *sender) {
    const self_t *mod = NULL;
    if (module_ref(self, modname, &mod) == 0) {
        return sender == mod;
    }
    return false;
}
