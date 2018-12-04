/* BEGIN_COMMON_COPYRIGHT_HEADER
 * 
 * clight: C daemon utility to automagically adjust screen backlight to match ambient brightness.
 * https://github.com/FedeDP/Clight/tree/master/clight
 *
 * Copyright (C) 2018  Federico Di Pierro <nierro92@gmail.com>
 *
 * This file is part of clight.
 * clight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * END_COMMON_COPYRIGHT_HEADER */

#include <modules.h>
#include <opts.h>

static void init(int argc, char *argv[]);
static void sigsegv_handler(int signum);
static void init_all_modules(void);
static void destroy(void);
static void main_poll(void);

struct state state;
struct config conf;
struct module modules[MODULES_NUM];
struct pollfd main_p[MODULES_NUM];

int main(int argc, char *argv[]) {
    state.quit = setjmp(state.quit_buf);
    if (!state.quit) {
        init(argc, argv);
        main_poll();
    }
    destroy();
    return state.quit == NORM_QUIT ? EXIT_SUCCESS : EXIT_FAILURE;
}

/*
 * First of all loads options from both global and 
 * local config file, and from cmdline options.
 * Then init needed modules.
 */
static void init(int argc, char *argv[]) {
    // when receiving segfault signal,
    // call our sigsegv handler that just logs
    // a debug message before dying
    signal(SIGSEGV, sigsegv_handler);

    init_opts(argc, argv);
    open_log();
    log_conf();
    init_all_modules();
}

/*
 * If received a sigsegv, log a message, destroy lock then
 * set sigsegv signal handler to default (SIG_DFL),
 * and send again the signal to the process.
 */
static void sigsegv_handler(int signum) {
    WARN("Received sigsegv signal. Aborting.\n");
    close_log();
    signal(signum, SIG_DFL);
    kill(getpid(), signum);
}

/* 
 * Init every module 
 */
static void init_all_modules(void) {
    for (int i = 0; i < MODULES_NUM; i++) {
        init_modules(i);
    }
}

/*
 * Free every used resource
 */
static void destroy(void) {
    /*
     * Avoid continuously cyclying here if any error happens inside destroy_modules() 
     * (as ERROR macro would longjmp again here);
     * Only try once, otherwise avoid destroying modules and go on closing log
     */
    static int first_time = 1;

    if (first_time) {
        first_time = 0;
        destroy_modules();
    }
    close_log();
}

/*
 * Listens on all fds and calls correct callback
 */
static void main_poll(void) {
    /* Force a sd_bus_process before entering loop */
    poll_cb(BUS);
    poll_cb(USERBUS);
    while (!state.quit) {
        int r = poll(main_p, MODULES_NUM, -1);
        if (r == -1 && errno != EINTR && errno != EAGAIN) {
            ERROR("%s\n", strerror(errno));
        }

        for (int i = 0; i < MODULES_NUM && !state.quit && r > 0; i++) {
            if (main_p[i].revents & POLLIN) {
                poll_cb(i);
                r--;
            }
        }
    }
}
