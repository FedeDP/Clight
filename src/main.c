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

#include <glob.h>
#include "opts.h"

static int init(int argc, char *argv[]);
static void init_state(void);
static void sigsegv_handler(int signum);
static int check_clightd_version(void);
static void init_user_mod_path(enum CONFIG file, char *filename);
static void load_user_modules(enum CONFIG file);

state_t state = {0};
conf_t conf = {0};

/* Every module needs these; let's init them before any module */
void modules_pre_start(void) {
    state.display = getenv("DISPLAY");
    state.wl_display = getenv("WAYLAND_DISPLAY");
    state.xauthority = getenv("XAUTHORITY");
} 

int main(int argc, char *argv[]) {
    int ret = EXIT_FAILURE;
    if (init(argc, argv) == 0) {
        if (conf.bl_conf.disabled && conf.dim_conf.disabled && conf.dpms_conf.disabled && conf.gamma_conf.disabled) {
            WARN("No functional module running. Leaving...\n");
        } else {
            ret = modules_loop();
        }
    }
    close_log();
    return ret;
}

/*
 * First of all loads options from both global and
 * local config file, and from cmdline options.
 * Then init needed modules.
 */
static int init(int argc, char *argv[]) {
    /* 
     * When receiving segfault signal,
     * call our sigsegv handler that just logs
     * a debug message before dying
     */
    signal(SIGSEGV, sigsegv_handler);
        
    open_log();
    /* We want any issue while parsing config to be logged */
    if (init_opts(argc, argv) != 0) {
        return -1;
    }
    log_conf();
    
    if (!conf.wizard) {
        /* We want any error while checking Clightd required version to be logged AFTER conf logging */
        if (check_clightd_version() != 0) {
            return -1;
        }
        
        init_state();
        /* 
        * Load user custom modules after opening log (thus this information is logged).
        * Note that local (ie: placed in $HOME) modules have higher priority,
        * thus one can override a global module (placed in /usr/share/clight/modules.d/)
        * by creating a module with same name in $HOME.
        * 
        * Clight internal modules cannot be overriden.
        */
        load_user_modules(LOCAL);
        load_user_modules(GLOBAL);
    }
    return 0;
}

static void init_state(void) {
    strncpy(state.version, VERSION, sizeof(state.version));
    memcpy(&state.current_loc, &conf.day_conf.loc, sizeof(loc_t));
    
    /* 
     * Initial states -> undefined; 
     */
    state.sens_avail = -1;
    state.next_event = -1;
    state.day_time = -1;
    state.ac_state = -1;
    state.lid_state = -1;
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
    raise(signum);
}

static int check_clightd_version(void) {
    SYSBUS_ARG(vers_args, CLIGHTD_SERVICE, "/org/clightd/clightd", "org.clightd.clightd", "Version");
    
    int r = get_property(&vers_args, "s", state.clightd_version, sizeof(state.clightd_version));
    if (r < 0 || !strlen(state.clightd_version)) {
        ERROR("No clightd found. Clightd is a mandatory dep.\n");
        r = -1;
    } else {
        int maj_val = atoi(state.clightd_version);
        int min_val = atoi(strchr(state.clightd_version, '.') + 1);
        if (maj_val < MINIMUM_CLIGHTD_VERSION_MAJ || (maj_val == MINIMUM_CLIGHTD_VERSION_MAJ && min_val < MINIMUM_CLIGHTD_VERSION_MIN)) {
            ERROR("Clightd must be updated. Required version: %d.%d.\n", MINIMUM_CLIGHTD_VERSION_MAJ, MINIMUM_CLIGHTD_VERSION_MIN);
            r = -1;
        } else {
            INFO("Clightd found, version: %s.\n", state.clightd_version);
        }
    }
    return r;
}

static void init_user_mod_path(enum CONFIG file, char *filename) {
    switch (file) {
        case LOCAL:
            if (getenv("XDG_DATA_HOME")) {
                snprintf(filename, PATH_MAX, "%s/clight/modules.d/*", getenv("XDG_DATA_HOME"));
            } else {
                snprintf(filename, PATH_MAX, "%s/.local/share/clight/modules.d/*", getpwuid(getuid())->pw_dir);
            }
            break;
        case GLOBAL:
            snprintf(filename, PATH_MAX, "%s/modules.d/*", DATADIR);
            break;
        default:
            break;
    }    
}

static void load_user_modules(enum CONFIG file) {
    char modules_path[PATH_MAX + 1];
    init_user_mod_path(file, modules_path);
    
    glob_t gl = {0};
    if (glob(modules_path, GLOB_NOSORT | GLOB_ERR, NULL, &gl) == 0) {
        for (int i = 0; i < gl.gl_pathc; i++) {
            if (m_load(gl.gl_pathv[i]) == MOD_OK) {
                INFO("'%s' loaded.\n", gl.gl_pathv[i]);
            } else {
                WARN("'%s' failed to load.\n", gl.gl_pathv[i]);
            }
        }
        globfree(&gl);
    }
}
