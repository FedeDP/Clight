#include "log.h"

double get_distance(struct location loc1, struct location loc2);
double degToRad(double angleDeg);
double radToDeg(double angleRad);
double compute_average(double *intensity, int num);
void polynomialfit(enum ac_states s);
double clamp(double value, double max, double min);
