#pragma once

#include "commons.h"

#define VALIDATE_REQ(X) _Generic((X), \
    loc_upd *: validate_loc, \
    upower_upd *: validate_upower, \
    inhibit_upd *: validate_inhibit, \
    timeout_upd *: validate_timeout, \
    contrib_upd *: validate_contrib, \
    evt_upd *: validate_evt, \
    temp_upd *: validate_temp, \
    calib_upd *: validate_autocalib, \
    curve_upd *: validate_curve, \
    bl_upd *: validate_backlight, \
    display_upd *: validate_display, \
    lid_upd *: validate_lid, \
    pm_upd *: validate_pm, \
    suspend_upd *: validate_suspend, \
    default: validate_nothing)(X)

bool validate_loc(loc_upd *up);
bool validate_upower(upower_upd *up);
bool validate_timeout(timeout_upd *up);
bool validate_inhibit(inhibit_upd *up);
bool validate_contrib(contrib_upd *up);
bool validate_evt(evt_upd *up);
bool validate_temp(temp_upd *up);
bool validate_autocalib(calib_upd *up);
bool validate_curve(curve_upd *up);
bool validate_backlight(bl_upd *up);
bool validate_display(display_upd *up);
bool validate_lid(lid_upd *up);
bool validate_pm(pm_upd *up);
bool validate_suspend(suspend_upd *up);
bool validate_nothing(void *up);
