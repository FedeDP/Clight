#include "utils.h"

const char *fetch_display() {
    if (state.display && strlen(state.display)) {
        return state.display;
    }
    return state.wl_display;
}

const char *fetch_env() {
    if (state.xauthority && strlen(state.xauthority)) {
        return state.xauthority;
    }
    return state.xdg_runtime_dir;
}

bool mod_check_pause(bool pause, int *paused_state, enum mod_pause reason, const char *modname) {
    int old_paused = *paused_state;
    if (pause) {
        *paused_state |= reason;
        if (old_paused == UNPAUSED) {
            DEBUG("Pausing %s\n", modname);
        }
        return old_paused == UNPAUSED;
    }
    *paused_state &= ~reason;
    if (old_paused != UNPAUSED && *paused_state == UNPAUSED) {
        DEBUG("Resuming %s\n", modname);
        return true;
    }
    return false;
}
