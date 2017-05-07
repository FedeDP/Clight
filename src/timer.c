#include "../inc/timer.h"

/*
 * Create timer and returns its fd to
 * the main struct pollfd
 */
int start_timer(int clockid, int initial_s, int initial_ns) {
    int timerfd = timerfd_create(clockid, 0);
    if (timerfd == -1) {
        ERROR("could not start timer: %s\n", strerror(errno));
    } else {
        set_timeout(initial_s, initial_ns, timerfd, 0);
    }
    return timerfd;
}

/*
 * Helper to set a new trigger on timerfd in sec seconds and n nsec
 */
void set_timeout(int sec, int nsec, int fd, int flag) {
    struct itimerspec timerValue = {{0}};

    timerValue.it_value.tv_sec = sec;
    timerValue.it_value.tv_nsec = nsec;
    timerValue.it_interval.tv_sec = 0;
    timerValue.it_interval.tv_nsec = 0;
    int r = timerfd_settime(fd, flag, &timerValue, NULL);
    if (r == -1) {
        return ERROR("%s\n", strerror(errno));
    }
    if (flag == 0) {
        DEBUG("Setted timeout of %ds %dns on %d fd.\n", sec, nsec, fd);
    }
}

unsigned int get_timeout(int fd) {
    struct itimerspec curr_value;
    timerfd_gettime(fd, &curr_value);
    return curr_value.it_value.tv_sec;
}

void reset_timer(int fd, int old_timer) {
    unsigned int elapsed_time = old_timer - get_timeout(fd);
    /* if we still need to wait some seconds */
    if (conf.timeout[state.ac_state][state.time] > elapsed_time) {
        set_timeout(conf.timeout[state.ac_state][state.time] - elapsed_time, 0, fd, 0);
    } else {
        /* with new timeout, old_timeout would already been elapsed */
        set_timeout(0, 1, fd, 0);
    }
}
