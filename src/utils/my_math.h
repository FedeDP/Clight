#include <log.h>

double degToRad(double angleDeg);
double radToDeg(double angleRad);
double compute_average(double *intensity, int num);
void polynomialfit(enum ac_states s);
double clamp(double value, double max, double min);
int calculate_sunrise(const float lat, const float lng, time_t *tt, int tomorrow) ;
int calculate_sunset(const float lat, const float lng, time_t *tt, int tomorrow);
