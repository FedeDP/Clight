#pragma once

#include "commons.h"

#define MODULE_WITH_PAUSE(name) \
    static int paused_state; \
    MODULE(name)

#define CHECK_PAUSE(pause, reason, name) mod_check_pause(pause, &paused_state, reason, name)

#define X_FIELDS \
    X(UNPAUSED,     0x00) \
    X(DISPLAY,      0x01) \
    X(SENSOR,       0x02) \
    X(AUTOCALIB,    0x04) \
    X(LID,          0x08) \
    X(SUSPEND,      0x10) \
    X(TIMEOUT,      0x20) \
    X(INHIBIT,      0x40) \
    X(CONTRIB,      0x80)

enum mod_pause {
#define X(name, value) name = value,
    X_FIELDS
#undef X
};

const char *fetch_display();
const char *fetch_env();
bool own_display(const char *display);
bool mod_check_pause(bool pause, int *paused_state, enum mod_pause reason, const char *modname);
