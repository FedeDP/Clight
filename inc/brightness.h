#include <turbojpeg.h>

#include "log.h"

void init_brightness(void);
double compute_brightness(unsigned int length);
double set_brightness(double perc);
void free_brightness(void);
