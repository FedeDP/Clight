#include <stdio.h>
#include <wand/MagickWand.h>
#include "log.h"

void init_brightness(void);
float compute_brightness(unsigned int length);
float set_brightness(float perc);
void free_wand(void);
