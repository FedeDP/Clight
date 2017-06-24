#include "../inc/lock.h"
#include <sys/file.h>

static int lck_fd; // fd on which clight enforces a lock to disable multiple clight instances running
static char lockfile[PATH_MAX + 1]; // our lock file -> $HOME/.clight.lock

void gain_lck(void) {
    snprintf(lockfile, PATH_MAX, "%s/.clight.lock", getpwuid(getuid())->pw_dir);
    lck_fd = open(lockfile, O_RDWR | O_CREAT);
    if (lck_fd == -1) {
        ERROR("Failed to open lockfile.\n");
    }
    if (flock(lck_fd, LOCK_EX | LOCK_NB) == -1) {
        close(lck_fd);
        ERROR("Failed to acquire lock.\n");
    }
    close(lck_fd);
}

void destroy_lck(void) {
    if (lck_fd > 0) {
        remove(lockfile);
        flock(lck_fd, LOCK_UN);
    }
}

