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
#include "../inc/gamma_smooth.h"
#include "../inc/location.h"
#include "../inc/signal.h"
#include "../inc/dpms.h"
#include "../inc/opts.h"
#include "../inc/lock.h"
#include "../inc/upower.h"
#include "../inc/dimmer.h"
#include "../inc/dimmer_smooth.h"
#include "../inc/xorg.h"
#include "../inc/inhibit.h"
#include "../inc/userbus.h"

static void init(int argc, char *argv[]);
static void set_modules_selfs(void);
static void init_all_modules(void);
static void destroy(void);
static void main_poll(void);

/*
 * pointers to init modules functions;
 */
static void (*const set_selfs[MODULES_NUM])(void) = {
    set_brightness_self, set_location_self, set_upower_self, set_gamma_self,
    set_gamma_smooth_self, set_signal_self, set_bus_self, set_dimmer_self,
    set_dimmer_smooth_self, set_dpms_self, set_xorg_self, set_inhibit_self, set_userbus_self
};

int main(int argc, char *argv[]) {
    state.quit = setjmp(state.quit_buf);
    if (!state.quit) {
        init(argc, argv);
        main_poll();
    }
    destroy();
    return state.quit >= ERR_QUIT ? EXIT_FAILURE : EXIT_SUCCESS;
}

/*
 * First of all loads options from both global and 
 * local config file, and from cmdline options.
 * Then init needed modules.
 */
static void init(int argc, char *argv[]) {
    init_opts(argc, argv);
    gain_lck();
    open_log();
    log_conf();
    set_modules_selfs();
    init_all_modules();
}

/*
 * Set each module self struct
 */
static void set_modules_selfs(void) {
    for (int i = 0; i < MODULES_NUM; i++) {
        set_selfs[i]();
    }
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
    if (state.quit <= ERR_QUIT) {
        destroy_modules();
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
            ERROR("%s\n", strerror(errno));
        }

        for (int i = 0; i < MODULES_NUM && r > 0; i++) {
            if (main_p[i].revents & POLLIN) {
                poll_cb(i);
                r--;
            }
        }
    }
}
