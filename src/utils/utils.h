#pragma once

#include "commons.h"

#define MODULE_WITH_PAUSE(name) \
    static int paused_state; \
    MODULE(name)

#define CHECK_PAUSE(pause, reason, name) mod_check_pause(pause, &paused_state, reason, name)

#define X_FIELDS \
    X(UNPAUSED,     0) \
    X(DISPLAY,      1 << 0) \
    X(SENSOR,       1 << 1) \
    X(AUTOCALIB,    1 << 2) \
    X(LID,          1 << 3) \
    X(SUSPEND,      1 << 4) \
    X(TIMEOUT,      1 << 5) \
    X(INHIBIT,      1 << 6) \
    X(CONTRIB,      1 << 7)

enum mod_pause {
#define X(name, value) name = value,
    X_FIELDS
#undef X
};

const char *fetch_display();
const char *fetch_env();
bool own_display(const char *display);
bool mod_check_pause(bool pause, int *paused_state, enum mod_pause reason, const char *modname);
