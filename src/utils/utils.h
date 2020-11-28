#pragma once

#include "commons.h"

#define MODULE_WITH_PAUSE(name) \
    static int paused_state; \
    MODULE(name)

#define CHECK_PAUSE(pause, reason, name) mod_check_pause(pause, &paused_state, reason, name)
    
enum mod_pause { UNPAUSED = 0, DISPLAY = 0x01, SENSOR = 0x02, AUTOCALIB = 0x04, LID = 0x08, SUSPEND = 0x10, TIMEOUT = 0x20, INHIBIT = 0x40, CONTRIB = 0x80 };

const char *fetch_display();
const char *fetch_env();
bool mod_check_pause(bool pause, int *paused_state, enum mod_pause reason, const char *modname);
