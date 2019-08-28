#pragma once

#include "commons.h"

int start_timer(int clockid, int initial_s, int initial_ns);
void set_timeout(int sec, int nsec, int fd, int flag);
void reset_timer(int fd, int old_timer, int new_timer);
void read_timer(int fd);
