#define _GNU_SOURCE

#include "../inc/bus.h"
#include "../inc/brightness.h"
#include "../inc/gamma.h"
#include "../inc/location.h"
#include "../inc/signal.h"
#include "../inc/dpms.h"
#include "../inc/opts.h"

static void init(int argc, char *argv[]);
static void destroy(void);
static void main_poll(void);

/*
 * pointers to init modules functions;
 */
static void (*const init_module[MODULES_NUM])(void) = {
    init_brightness, init_gamma, init_location, init_signal, init_dpms
};

/*
 * pointers to destroy modules functions;
 */
static void (*const destroy_module[MODULES_NUM])(void) = {
    destroy_brightness, destroy_gamma, destroy_location, destroy_signal, destroy_dpms
};


int main(int argc, char *argv[]) {
    init(argc, argv);
    main_poll();
    destroy();
    return 0;
}

/*
 * Creates every needed struct/variable.
 */
static void init(int argc, char *argv[]) {
    open_log();
    init_opts(argc, argv);
    init_bus();
    for (int i = 0; i < MODULES_NUM && !state.quit; i++) {
        init_module[i]();
    }
    if (!state.quit && conf.single_capture_mode) {
        main_p.cb[CAPTURE_IX]();
        state.quit = 1;
    }
}

/**
 * Free every used resource
 */
static void destroy(void) {
    for (int i = 0; i < MODULES_NUM; i++) {
        destroy_module[i]();
    }
    destroy_bus();
    close_log();
}

/*
 * Listens on all fds and calls correct callback
 */
static void main_poll(void) {
    while (!state.quit) {
        int r = poll(main_p.p, MODULES_NUM - 1, -1);
        if (r == -1) {
            if (errno == EINTR) {
                continue;
            }
            state.quit = 1;
            return;
        }

        for (int i = 0; i < MODULES_NUM - 1 && r > 0; i++) {
            if (main_p.p[i].revents & POLLIN) {
                main_p.cb[i]();
                r--;
            }
        }
    }
}
