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
    const int limit = conf.single_capture_mode ? 1 : MODULES_NUM;
    for (int i = 0; i < limit && !state.quit; i++) {
        init_module[i]();
    }
}

/**
 * Free every used resource
 */
static void destroy(void) {
    const int limit = conf.single_capture_mode ? 1 : MODULES_NUM;
    for (int i = 0; i < limit; i++) {
        destroy_module[i]();
    }
    destroy_bus();
    destroy_opts();
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
