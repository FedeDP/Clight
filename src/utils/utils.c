#include "commons.h"

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
