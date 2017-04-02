#pragma once

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/limits.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>

#define SIZE(a) (sizeof(a) / sizeof(*a))

enum states { DAY, NIGHT };

struct config {
    int num_captures;
    int timeout[2];
    char dev_name[PATH_MAX + 1];
    char screen_path[PATH_MAX + 1];
    int temp[2];
    int smooth_transition;
    double lat;
    double lon;
};

struct state {
    int quit;
    double *values;
    enum states time; // whether it is day or night time
};

struct state state;
struct config conf;
int camera_width, camera_height;
uint8_t *buffer;


