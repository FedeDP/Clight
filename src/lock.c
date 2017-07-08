#include "../inc/lock.h"
#include <sys/file.h>

static int lck_fd; // fd on which clight enforces a lock to disable multiple clight instances running
static char lockfile[PATH_MAX + 1]; // our lock file -> $HOME/.clight.lock

void gain_lck(void) {
    if (conf.single_capture_mode) {
        snprintf(lockfile, PATH_MAX, "%s/.clight_capture.lock", getpwuid(getuid())->pw_dir);
    } else {
        snprintf(lockfile, PATH_MAX, "%s/.clight.lock", getpwuid(getuid())->pw_dir);
    }
    lck_fd = open(lockfile, O_RDWR | O_CREAT);
    if (lck_fd == -1) {
        ERROR("%s\n", strerror(errno));
    }
    if (flock(lck_fd, LOCK_EX | LOCK_NB) == -1) {
        close(lck_fd);
        ERROR("%s\n", strerror(errno));
    }
    close(lck_fd);
}

void destroy_lck(void) {
    if (lck_fd > 0) {
        remove(lockfile);
        flock(lck_fd, LOCK_UN);
    }
}

