/* BEGIN_COMMON_COPYRIGHT_HEADER
 *
 * clight: C daemon utility to automagically adjust screen backlight to match ambient brightness.
 * https://github.com/FedeDP/Clight/tree/master/clight
 *
 * Copyright (C) 2019  Federico Di Pierro <nierro92@gmail.com>
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

#include <module/modules_easy.h>
#include <opts.h>
#include <log.h>

static void init(int argc, char *argv[]);
static void sigsegv_handler(int signum);

struct state state;
struct config conf;
struct module modules[MODULES_NUM];
struct pollfd main_p[MODULES_NUM];
sd_bus *sysbus, *userbus;

/* Every module needs these; let's init them before any module */
void modules_pre_start(void) {
    state.display = getenv("DISPLAY");
    state.wl_display = getenv("WAYLAND_DISPLAY");
    state.xauthority = getenv("XAUTHORITY");
    
    sd_bus_default_system(&sysbus);
    sd_bus_default_user(&userbus);
} 

int main(int argc, char *argv[]) {
    state.quit = setjmp(state.quit_buf);
    if (!state.quit) {
        init(argc, argv);
        modules_loop();
    }
    close_log();
    return state.quit == NORM_QUIT ? EXIT_SUCCESS : EXIT_FAILURE;
}

/*
 * First of all loads options from both global and
 * local config file, and from cmdline options.
 * Then init needed modules.
 */
static void init(int argc, char *argv[]) {
    /* 
     * When receiving segfault signal,
     * call our sigsegv handler that just logs
     * a debug message before dying
     */
    signal(SIGSEGV, sigsegv_handler);
    init_opts(argc, argv);
    open_log();
    log_conf();
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
