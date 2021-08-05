#include "validations.h"
#include "my_math.h"

bool validate_loc(loc_upd *up) {
    if (fabs(up->new.lat) <  90.0f && fabs(up->new.lon) < 180.0f && 
        get_distance(&up->new, &state.current_loc) >= LOC_DISTANCE_THRS) {

        return true;
    }
    DEBUG("Failed to validate location request.\n");
    return false;
}

bool validate_upower(upower_upd *up) {
    if (state.ac_state != up->new && up->new >= ON_AC && up->new < SIZE_AC) {
        return true;
    }
    DEBUG("Failed to validate upower request.\n");
    return false;
}

bool validate_timeout(timeout_upd *up) {
    if (up->state == -1) {
        up->state = state.ac_state;
    }
    
    if (up->daytime == -1) {
        up->daytime = state.day_time;
    }
    
    if (up->state >= ON_AC && up->state < SIZE_AC) {
        return true;
    }
    DEBUG("Failed to validate timeout request.\n");
    return false;
}

bool validate_inhibit(inhibit_upd *up) {
    if (state.inhibited != up->new) {
        return true;
    }
    DEBUG("Failed to validate inhibit request.\n");
    return false;
}

bool validate_contrib(contrib_upd *up) {
    if (up->new >= 0.0 && up->new <= 1.0 && conf.screen_conf.contrib != up->new) {
        return true;
    }
    DEBUG("Failed to validate contrib request.\n");
    return false;
}

bool validate_evt(evt_upd *up) {
    struct tm timeinfo;
    // Accept 0-len string to reset event
    const size_t len = strlen(up->event);
    if (len == 0 || 
        (len < sizeof(conf.day_conf.day_events[SUNRISE]) &&
        strptime(up->event, "%R", &timeinfo))) {
     
        return true;
    }
    DEBUG("Failed to validate sunrise/sunset request.\n");
    return false;
}

bool validate_temp(temp_upd *up) {
    if (up->daytime == -1) {
        up->daytime = state.day_time;
    }
    
    if (up->smooth == -1) {
        up->smooth = !conf.gamma_conf.no_smooth;
        up->step = conf.gamma_conf.trans_step;
        up->timeout = conf.gamma_conf.trans_timeout;
    }
    
    /* Validate new temp: check clighd limits */
    if (up->new >= 1000 && up->new <= 10000 && 
        up->daytime >= DAY && up->daytime < SIZE_STATES) {
     
        return true;
    }
    DEBUG("Failed to validate temperature request.\n");
    return false;
}

bool validate_autocalib(calib_upd *up) {
    if (conf.bl_conf.no_auto_calib != up->new) {
        return true;
    }
    DEBUG("Failed to validate autocalib request.\n");
    return false;
}

bool validate_curve(curve_upd *up) {
    if (up->state == -1) {
        up->state = state.ac_state;
    }
    
    if (up->state >= ON_AC && up->state < SIZE_AC &&
        up->num_points > 0 && up->num_points <= MAX_SIZE_POINTS &&
        up->regression_points != NULL) {
        return true;
    }
    DEBUG("Failed to validate curve request.\n");
    return false;
}

bool validate_backlight(bl_upd *up) {
    if (up->smooth == -1) {
        up->smooth = !conf.bl_conf.no_smooth;
        up->step = conf.bl_conf.trans_step;
        up->timeout = conf.bl_conf.trans_timeout;
    }
    
    if (up->new >= 0.0 && up->new <= 1.0) {
        return true;
    }
    DEBUG("Failed to validate backlight level request.\n");
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
    DEBUG("Failed to validate display state request.\n");
    return false;
}

bool validate_lid(lid_upd *up) {
    if (up->new >= OPEN &&
        up->new <= DOCKED &&
        state.lid_state != up->new) {
        
        return true;
    }
    DEBUG("Failed to validate lid request.\n");
    return false;
}

bool validate_pm(pm_upd *up) {
    if (up->new != state.pm_inhibited) {
        return true;
    }
    DEBUG("Failed to validate pm request.\n");
    return false;
}

bool validate_suspend(suspend_upd *up) {
    if (up->new != state.suspended) {
        return true;
    }
    DEBUG("Failed to validate suspend request.\n");
    return false;
}

bool validate_ambgamma(ambgamma_upd *up) {
    if (up->new != conf.gamma_conf.ambient_gamma) {
        return true;
    }
    DEBUG("Failed to validate ambgamma request.\n");
    return false;
}

bool validate_nothing(void *up) {
    return true;
}
