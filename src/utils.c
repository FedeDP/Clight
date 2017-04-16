#include "../inc/utils.h"

static const char *dict[MODULES_NUM] = {"Brightness", "Location", "Gamma", "Signal", "Dpms"};

/**
 * Create timer and returns its fd to
 * the main struct pollfd
 */
int start_timer(int clockid, int initial_timeout) {
    int timerfd = timerfd_create(clockid, 0);
    if (timerfd == -1) {
        ERROR("could not start timer: %s\n", strerror(errno));
        state.quit = 1;
    } else {
        set_timeout(initial_timeout, 0, timerfd, 0);
    }
    return timerfd;
}

/**
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
        state.quit = 1;
    }
}

void init_module(int fd, enum modules module, void (*cb)(void), void (*destroy_func)(void)) {
    if (fd == -1) {
        state.quit = 1;
        return;
    }

    main_p[module] = (struct pollfd) {
        .fd = fd,
        .events = POLLIN,
    };
    modules[module].destroy = destroy_func;
    modules[module].poll_cb = cb;
    
    /* 
     * if fd==DONT_POLL_W_ERR, it means a not-critical error happened
     * while module was setting itself up, before calling init_module.
     * eg: geoclue2 support is enabled but geoclue2 could not be found.
     */
    if (fd != DONT_POLL_W_ERR) {
        modules[module].inited = 1;
        INFO("%s module started.\n", dict[module]);
    }
}

void destroy_module(enum modules module) {
    /* 
     * Check even if destroy is a valid pointer. 
     * For now every module has a destroy func,
     * but in the future they may not.
     */
    if (modules[module].inited && modules[module].destroy) {
        modules[module].destroy();
        INFO("%s module destroyed.\n", dict[module]);
    }
}
