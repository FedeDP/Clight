#include "modules.h"
#include <sys/timerfd.h>

int start_timer(int clockid, int initial_s, int initial_ns);
void set_timeout(int sec, int nsec, int fd, int flag);
unsigned int get_timeout(int fd);
void reset_timer(int fd, int old_timer);
