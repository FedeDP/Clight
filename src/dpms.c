#include "../inc/dpms.h"

/*
 * info->power_level is one of:
 * DPMS Extension Power Levels
 * 0     DPMSModeOn          In use
 * 1     DPMSModeStandby     Blanked, low power
 * 2     DPMSModeSuspend     Blanked, lower power
 * 3     DPMSModeOff         Shut off, awaiting activity
 * 
 * Clightd returns -1 if dpms is disabled and an error if we're not on X
 */
int get_screen_dpms(void) {
    int dpms_state = -1;
    
    if (state.display && state.xauthority) {
        struct bus_args args_get = {"org.clightd.backlight", "/org/clightd/backlight", "org.clightd.backlight", "getdpms"};
        bus_call(&dpms_state, "i", &args_get, "ss", state.display, state.xauthority);
    }
    return dpms_state;
}
