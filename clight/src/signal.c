#include <sys/signalfd.h>
#include <signal.h>
#include "../inc/signal.h"

static void signal_cb(void);

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
    set_pollfd(fd, SIGNAL_IX, signal_cb);
    INFO("Signal module started.\n");
}

/*
 * if received an external SIGINT or SIGTERM,
 * just switch the quit flag to 1 and print to stdout.
 */
static void signal_cb(void) {
    struct signalfd_siginfo fdsi;
    ssize_t s;

    s = read(main_p.p[SIGNAL_IX].fd, &fdsi, sizeof(struct signalfd_siginfo));
    if (s != sizeof(struct signalfd_siginfo)) {
        ERROR("an error occurred while getting signalfd data.\n");
    } else {
        INFO("\nreceived signal %d. Leaving.\n", fdsi.ssi_signo);
    }
    state.quit = 1;
}

void destroy_signal(void) {
    if (main_p.p[SIGNAL_IX].fd > 0) {
        close(main_p.p[SIGNAL_IX].fd);
    }
    INFO("Signal module destroyed.\n");
}
