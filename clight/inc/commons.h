#pragma once

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/limits.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

struct config {
    int num_captures;
    int timeout;
    char dev_name[PATH_MAX + 1];
    char screen_path[PATH_MAX + 1];
};

struct config conf;
int quit, camera_width, camera_height;
uint8_t *buffer;


