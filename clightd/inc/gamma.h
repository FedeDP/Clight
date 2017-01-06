/**
 * Taken from sct.c
 * http://www.tedunangst.com/flak/post/sct-set-color-temperature
 */

#ifndef DISABLE_GAMMA

#include <X11/extensions/Xrandr.h>

#include <math.h>

#include "commons.h"

void set_gamma(int temp, int *err);
int get_gamma(int *err);

#endif
