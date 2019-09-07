#include "validations.h"

bool validate_loc(loc_upd *up) {
    if (fabs(up->new.lat) <  90.0f && fabs(up->new.lon) < 180.0f && 
        (up->new.lat != state.current_loc.lat || up->new.lon != state.current_loc.lon)) {
        
        return true;
    }
    WARN("Failed to validate location request.\n");
    return false;
}

bool validate_upower(upower_upd *up) {
    if (state.ac_state != up->new && up->new >= ON_AC && up->new < SIZE_AC) {
        return true;
    }
    WARN("Failed to validate upower request.\n");
    return false;
}

bool validate_timeout(timeout_upd *up) {
    if (up->state >= ON_AC && up->state < SIZE_AC) {
        return true;
    }
    WARN("Failed to validate timeout request.\n");
    return false;
}

bool validate_inhibit(inhibit_upd *up) {
    if (state.inhibited != up->new) {
        return true;
    }
    WARN("Failed to validate inhibit request.\n");
    return false;
}

bool validate_contrib(contrib_upd *up) {
    if (up->new >= 0.0 && up->new <= 1.0 && conf.screen_contrib != up->new) {
        return true;   
    }
    WARN("Failed to validate contrib request.\n");
    return false;
}

bool validate_evt(evt_upd *up) {
    struct tm timeinfo;
    if (strlen(up->event) && 
        strlen(up->event) < sizeof(conf.day_events[SUNRISE]) &&
        strptime(up->event, "%R", &timeinfo)) {
     
        return true;
    }
    WARN("Failed to validate sunrise/sunset request.\n");
    return false;
}

bool validate_temp(temp_upd *up) {
    /* Validate new temp: check clighd limits */
    if (up->new >= 1000 && up->new <= 10000 && 
        up->daytime >= DAY && up->daytime < SIZE_STATES &&
        up->new != conf.temp[up->daytime]) {
     
        return true;
    }
    WARN("Failed to validate temperature request.\n");
    return false;
}

bool validate_autocalib(calib_upd *up) {
    if (conf.no_auto_calib != up->new) {
        return true;
    }
    WARN("Failed to validate autocalib request.\n");
    return false;
}

bool validate_curve(curve_upd *up) {
    if (up->state >= ON_AC && up->state < SIZE_AC &&
        up->num_points > 0 && up->num_points <= MAX_SIZE_POINTS) {
     
        return true;
    }
    WARN("Failed to validate curve request.\n");
    return false;
}

bool validate_backlight(bl_upd *up) {
    if (up->new >= 0.0 && up->new <= 1.0) {
        return true;
    }
    WARN("Failed to validate backlight level request.\n");
    return false;
}

bool validate_display(display_upd *up) {
    if (up->new >= DISPLAY_ON && up->new < DISPLAY_SIZE                              // check up->new validity.
        &&
        (((state.display_state & DISPLAY_OFF) && (up->new == DISPLAY_ON))            // if we are in DPMS, always require a DISPLAY_ON request even if we are DIMMED too.
        ||
        (!(state.display_state & DISPLAY_OFF) && up->new != state.display_state))) { // if we are !DPMS (ie: DIMMED or ON), request any value that is different from current one.
        
        return true;
    }
    WARN("Failed to validate display state request.\n");
    return false;
}

bool validate_nothing(void *up) {
    return true;
}
