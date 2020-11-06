#include <sys/signalfd.h>
#include <signal.h>
#include "commons.h"

MODULE("SIGNAL");

/*
 * Set signals handler for SIGINT, SIGTERM and SIGHUP (using a signalfd)
 * SIGHUP is sent by systemd upon leaving user session! 
 * See: https://fedoraproject.org/wiki/Changes/KillUserProcesses_by_default
 */
static void init(void) {
    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGHUP);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    int fd = signalfd(-1, &mask, 0);
    m_register_fd(fd, true, NULL);
}

static bool check(void) {
    return true;
}

static bool evaluate() {
    return true;
}


static void destroy(void) {
    /* Skeleton function needed for modules interface */
}

static void receive(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
    case FD_UPD: {
        struct signalfd_siginfo fdsi;
        ssize_t s;

        s = read(msg->fd_msg->fd, &fdsi, sizeof(struct signalfd_siginfo));
        if (s != sizeof(struct signalfd_siginfo)) {
            ERROR("An error occurred while getting signalfd data.\n");
        }
        INFO("Received %d. Leaving.\n", fdsi.ssi_signo);
        modules_quit(EXIT_SUCCESS);
        break;
    }
    default:
        break;
    }
}
