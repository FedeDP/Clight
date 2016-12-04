#include <systemd/sd-bus.h>

#include "commons.h"

void init_brightness(void);
double set_brightness(double perc);
double capture_frames(void);
void free_brightness(void);
