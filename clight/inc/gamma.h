#include <bits/time.h>
#include <math.h>
#include <time.h>

#include "bus.h"

struct time {
    time_t next_alarm;
    int state;
};

void check_gamma_time(const float lat, const float lon, struct time *t);
int set_screen_temp(int status);
