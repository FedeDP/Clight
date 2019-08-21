#include <sys/signalfd.h>
#include <signal.h>
#include <log.h>

MODULE("SIGNAL");

/*
 * Set signals handler for SIGINT and SIGTERM (using a signalfd)
 */
static void init(void) {
    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
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

/*
 * if received an external SIGINT or SIGTERM,
 * just switch the quit flag to 1 and print to stdout.
 */
static void receive(const msg_t *const msg, const void* userdata) {
    if (!msg->is_pubsub) {
        struct signalfd_siginfo fdsi;
        ssize_t s;

        s = read(msg->fd_msg->fd, &fdsi, sizeof(struct signalfd_siginfo));
        if (s != sizeof(struct signalfd_siginfo)) {
            ERROR("An error occurred while getting signalfd data.\n");
        }
        INFO("Received %d. Leaving.\n", fdsi.ssi_signo);
        modules_quit(NORM_QUIT);
    }
}

