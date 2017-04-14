/* BEGIN_COMMON_COPYRIGHT_HEADER
 * 
 * clight: C daemon utility to automagically adjust screen backlight to match ambient brightness.
 * https://github.com/FedeDP/Clight/tree/master/clight
 *
 * Copyright (C) 2017  Federico Di Pierro <nierro92@gmail.com>
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

#include "../inc/bus.h"
#include "../inc/brightness.h"
#include "../inc/gamma.h"
#include "../inc/location.h"
#include "../inc/signal.h"
#include "../inc/dpms.h"
#include "../inc/opts.h"

#include <sys/file.h>

static void gain_lck(void);
static void destroy_lck(void);
static void init(int argc, char *argv[]);
static void destroy(void);
static void main_poll(void);

/*
 * pointers to init modules functions;
 */
static void (*const init_m[MODULES_NUM])(void) = {
    init_brightness, init_location, init_gamma, init_signal, init_dpms
};
static int lck_fd; // fd on which clight enforces a lock to disable multiple clight instances running
static char lockfile[PATH_MAX + 1]; // our lock file -> $HOME/.clight.lock

int main(int argc, char *argv[]) {
    init(argc, argv);
    main_poll();
    destroy();
    return 0;
}

static void gain_lck(void) {
    snprintf(lockfile, PATH_MAX, "%s/.clight.lock", getpwuid(getuid())->pw_dir);
    lck_fd = open(lockfile, O_RDWR | O_CREAT);
    if (lck_fd == -1) { 
        state.quit = 1;
        ERROR("Failed to open lock.\n");
        return;
    }
    if (flock(lck_fd, LOCK_EX | LOCK_NB) == -1) { 
        ERROR("Failed to acquire lock.\n");
        state.quit = 1;
    }
    close(lck_fd);
}

static void destroy_lck(void) {
    if (lck_fd > 0) {
        remove(lockfile);
        flock(lck_fd, LOCK_UN);
    }
}

/*
 * Creates every needed struct/variable.
 */
static void init(int argc, char *argv[]) {
    open_log();
    init_opts(argc, argv);
    if (!conf.single_capture_mode) {
        gain_lck();
        if (state.quit) {
            return;
        }
    }
    init_bus();
    // do not init every module if we're doing a single capture
    const int limit = conf.single_capture_mode ? 1 : MODULES_NUM;
    for (int i = 0; i < limit && !state.quit; i++) {
        init_m[i]();
    }
}

/**
 * Free every used resource
 */
static void destroy(void) {
    for (int i = 0; i < MODULES_NUM; i++) {
        destroy_module(i);
    }
    destroy_bus();
    destroy_lck();
    destroy_opts();
    close_log();
}

/*
 * Listens on all fds and calls correct callback
 */
static void main_poll(void) {
    while (!state.quit) {
        int r = poll(main_p, MODULES_NUM, -1);
        if (r == -1) {
            if (errno == EINTR) {
                continue;
            }
            state.quit = 1;
            return;
        }

        for (int i = 0; i < MODULES_NUM && r > 0; i++) {
            /*
             * it should never happen that no cb is registered for a polled module.
             * dpms_module does not register an fd to be listened on poll.
             */
            if ((main_p[i].revents & POLLIN) && (modules[i].poll_cb)) {
                modules[i].poll_cb();
                r--;
            }
        }
    }
}
