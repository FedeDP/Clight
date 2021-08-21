#pragma once

#include "commons.h"

double degToRad(double angleDeg);
double radToDeg(double angleRad);
double compute_average(const double *intensity, int num);
void polynomialfit(double *XPoints, curve_t *curve, const char *tag);
double clamp(double value, double max, double min);
double get_value_from_curve(const double perc, curve_t *curve);
int calculate_sunrise(const float lat, const float lng, time_t *tt, int dayshift);
int calculate_sunset(const float lat, const float lng, time_t *tt, int dayshift);
double get_distance(loc_t *loc1, loc_t *loc2);
