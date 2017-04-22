#include <sys/signalfd.h>
#include <signal.h>
#include "../inc/signal.h"

static void signal_cb(void);

static struct self_t self = {
    .name = "Signal",
    .idx = SIGNAL_IX,
    .module = &modules[SIGNAL_IX],
};

/**
 * Set signals handler for SIGINT and SIGTERM (using a signalfd)
 */
void init_signal(void) {
    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    int fd = signalfd(-1, &mask, 0);
    init_module(fd, self.idx, signal_cb, destroy_signal, self.name);
}

/*
 * if received an external SIGINT or SIGTERM,
 * just switch the quit flag to 1 and print to stdout.
 */
static void signal_cb(void) {
    struct signalfd_siginfo fdsi;
    ssize_t s;

    s = read(main_p[self.idx].fd, &fdsi, sizeof(struct signalfd_siginfo));
    if (s != sizeof(struct signalfd_siginfo)) {
        return ERROR("an error occurred while getting signalfd data.\n");
    }
    INFO("received signal %d. Leaving.\n", fdsi.ssi_signo);
    state.quit = 1;
}

void destroy_signal(void) {
    if (main_p[self.idx].fd > 0) {
        close(main_p[self.idx].fd);
    }
    INFO("%s module destroyed.\n", self.name);
}
