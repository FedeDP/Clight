#pragma once

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/limits.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>

struct config {
    int num_captures;
    int timeout;
    char dev_name[PATH_MAX + 1];
    char screen_path[PATH_MAX + 1];
    int day_temp;
    int night_temp;
    int smooth_transition;
    double lat;
    double lon;
};

struct state {
    int quit;
    double *values;
};

struct state state;
struct config conf;
int camera_width, camera_height;
uint8_t *buffer;


