#include "../inc/utils.h"

/*
 * Create timer and returns its fd to
 * the main struct pollfd
 */
int start_timer(int clockid, int initial_ns, int initial_s) {
    int timerfd = timerfd_create(clockid, 0);
    if (timerfd == -1) {
        ERROR("could not start timer: %s\n", strerror(errno));
    } else {
        set_timeout(initial_s, initial_ns, timerfd, 0);
    }
    return timerfd;
}

/*
 * Helper to set a new trigger on timerfd in $start seconds
 */
void set_timeout(int sec, int nsec, int fd, int flag) {
    struct itimerspec timerValue = {{0}};

    timerValue.it_value.tv_sec = sec;
    timerValue.it_value.tv_nsec = nsec;
    timerValue.it_interval.tv_sec = 0;
    timerValue.it_interval.tv_nsec = 0;
    int r = timerfd_settime(fd, flag, &timerValue, NULL);
    if (r == -1) {
        ERROR("%s\n", strerror(errno));
    }
}

unsigned int get_timeout(int fd) {
    struct itimerspec curr_value;
    timerfd_gettime(fd, &curr_value);
    return curr_value.it_value.tv_sec;
}
