#include "modules.h"

int start_timer(int initial_s, int initial_ns);
void set_timeout(int sec, int nsec, int fd);
unsigned int get_timeout(int fd);
void reset_timer(int fd, int old_timer);
