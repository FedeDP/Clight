#include <sys/timerfd.h>
#include "log.h"

int start_timer(int clockid, int initial_s, int initial_ns);
void set_timeout(int sec, int nsec, int fd, int flag);
long get_timeout_sec(int fd);
// long get_timeout_nsec(int fd);
void reset_timer(int fd, int old_timer, int new_timer);
