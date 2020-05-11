#pragma once

#include "commons.h"

double degToRad(double angleDeg);
double radToDeg(double angleRad);
double compute_average(const double *intensity, int num);
void polynomialfit(double *XPoints, double *YPoints, double *out_params, int num_points);
double clamp(double value, double max, double min);
int calculate_sunrise(const float lat, const float lng, time_t *tt, int tomorrow) ;
int calculate_sunset(const float lat, const float lng, time_t *tt, int tomorrow);
double get_distance(loc_t *loc1, loc_t *loc2);
