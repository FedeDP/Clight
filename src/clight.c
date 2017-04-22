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
#include "../inc/lock.h"

static void init(int argc, char *argv[]);
static void destroy(void);
static void main_poll(void);

/*
 * pointers to init modules functions;
 */
static void (*const init_m[MODULES_NUM])(void) = {
    init_brightness, init_location, init_gamma, init_signal, init_dpms
};

int main(int argc, char *argv[]) {
    init(argc, argv);
    main_poll();
    destroy();
    return 0;
}

/*
 * First of all loads optiosn from both global and local config file,
 * and from cmdline options.
 * If we're not in single_capture_mode, it gains lock and opens log.
 * Then checks conf and init needed modules.
 */
static void init(int argc, char *argv[]) {
    init_opts(argc, argv);
    if (!conf.single_capture_mode && !state.quit) {
        gain_lck();
        if (!state.quit) { 
            open_log();
            log_conf();
        }
    }
    if (state.quit) {
        return;
    }
    check_conf();
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
        if (modules[i].inited && modules[i].destroy) {
            modules[i].destroy();
        }
    }
    destroy_bus();
    close_log();
    destroy_lck();
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
