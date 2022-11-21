#include <sys/signalfd.h>
#include <signal.h>
#include "commons.h"

DECLARE_MSG(suspend_req, SUSPEND_REQ);
DECLARE_MSG(capture_req, CAPTURE_REQ);

MODULE("SIGNAL");

/*
 * Set signals handler for SIGINT, SIGTERM and SIGHUP (using a signalfd)
 * SIGHUP is sent by systemd upon leaving user session! 
 * See: https://fedoraproject.org/wiki/Changes/KillUserProcesses_by_default
 */
static void init(void) {
    capture_req.capture.reset_timer = false;
    capture_req.capture.capture_only = false;
    
    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGHUP);
    sigaddset(&mask, SIGTSTP);
    sigaddset(&mask, SIGCONT);
    sigaddset(&mask, SIGUSR1);
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
        switch (fdsi.ssi_signo) {
        case SIGTSTP:
            suspend_req.suspend.new = true;
            M_PUB(&suspend_req);
            break;
        case SIGCONT:
            suspend_req.suspend.new = false;
            M_PUB(&suspend_req);
            break;
        case SIGUSR1:
            M_PUB(&capture_req);
            break;
        default:
            INFO("Received %d. Leaving.\n", fdsi.ssi_signo);
            modules_quit(EXIT_SUCCESS);
            break;
        }
        break;
    }
    default:
        break;
    }
}
