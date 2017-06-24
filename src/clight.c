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
#include "../inc/upower.h"
#include "../inc/dimmer.h"

static void init(int argc, char *argv[]);
static void set_modules_selfs(void);
static void init_all_modules(void);
static void destroy(void);
static void main_poll(void);

/*
 * pointers to init modules functions;
 */
static void (*const set_selfs[])(void) = {
    set_brightness_self, set_location_self, set_upower_self, 
    set_gamma_self, set_signal_self, set_bus_self, set_dimmer_self, set_dpms_self
};

int main(int argc, char *argv[]) {
    init(argc, argv);
    main_poll();
    destroy();
    return 0;
}

/*
 * First of all loads options from both global and 
 * local config file, and from cmdline options.
 * If we're not in single_capture_mode, it gains lock and opens log, logging current configuration.
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
    set_modules_selfs();
    if (!state.quit) {
        init_all_modules();
    }
}

/* 
 * Set each module self struct 
 */
static void set_modules_selfs(void) {
    for (int i = 0; i < MODULES_NUM && !state.quit; i++) {
        set_selfs[i]();
    }
}

/* 
 * Init every module 
 */
static void init_all_modules(void) {
    for (int i = 0; i < MODULES_NUM && !state.quit; i++) {
        init_modules(i);
    }
}

/*
 * Free every used resource
 */
static void destroy(void) {
    for (int i = 0; i < MODULES_NUM; i++) {
        destroy_modules(i);
    }
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
        }

        for (int i = 0; i < MODULES_NUM && r > 0; i++) {
            if (main_p[i].revents & POLLIN) {
                poll_cb(i);
                r--;
            }
        }
    }
}
