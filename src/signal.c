#include <sys/signalfd.h>
#include <signal.h>
#include "../inc/signal.h"

static void init(void);
static void destroy(void);
static void signal_cb(void);

static struct self_t self = {
    .name = "Signal",
    .idx = SIGNAL_IX,
};

void set_signal_self(void) {
    modules[self.idx].self = &self;
    modules[self.idx].init = init;
    modules[self.idx].destroy = destroy;
}

/**
 * Set signals handler for SIGINT and SIGTERM (using a signalfd)
 */
static void init(void) {    
    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    int fd = signalfd(-1, &mask, 0);
    init_module(fd, self.idx, signal_cb);
}

static void destroy(void) {
    if (main_p[self.idx].fd > 0) {
        close(main_p[self.idx].fd);
    }
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

