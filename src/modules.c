#include "../inc/modules.h"

static void started_cb(enum modules module);

/* 
 * Start a module only if it is not disabled, it is not inited, and a proper init hook function has been setted.
 * Check if all deps modules have been started too.
 * If module has not a poll_cb (it is not waiting on poll), call poll_cb right now as it is fully started already.
 */
void init_modules(const enum modules module) {
    /* Avoid calling init in case module is disabled, is already inited, or init func ptr is NULL */
    if (!modules[module].disabled && modules[module].init && !modules[module].inited) {
        if (modules[module].self->num_deps == modules[module].self->satisfied_deps) {
            if (modules[module].check()) {
                disable_module(module);
            } else {
                modules[module].init();
            }
        }
    }
}

void init_module(int fd, enum modules module, void (*cb)(void)) {
    if (fd == -1) {
        state.quit = 1;
        return;
    }
    
    main_p[module] = (struct pollfd) {
        .fd = fd,
        .events = POLLIN,
    };
    modules[module].poll_cb = cb;
    
    /* 
     * if fd==DONT_POLL_W_ERR, it means a not-critical error happened
     * while module was setting itself up, before calling init_module.
     * eg: geoclue2 support is enabled but geoclue2 could not be found.
     */
    if (fd != DONT_POLL_W_ERR) {
        modules[module].inited = 1;
        INFO("%s module started.\n", modules[module].self->name);
    } else {
        /* module should be disabled */
        WARN("Error while loading %s module.\n", modules[module].self->name);
        disable_module(module); // disable this module and all of dependent module
    }
}

/*
 * Foreach dep, set self as dependent on that module
 */
void set_self_deps(struct self_t *self) {
    if (!modules[self->idx].disabled) {
        for (int i = 0; i < self->num_deps; i++) {
            enum modules m = self->deps[i].dep;
            modules[m].num_dependent++;
            modules[m].dependent_m = realloc(modules[m].dependent_m, modules[m].num_dependent * sizeof(modules[m].dependent_m));
            if (!modules[m].dependent_m) {
                ERROR("%s\n", strerror(errno));
                break;
            }
            modules[m].dependent_m[modules[m].num_dependent - 1] = self->idx;
        }
    }
}

/*
 * Callback called when a module that is needed by some others
 * gets started: it increment satisfied_deps for each dependent modules
 * and tries to start them.
 * If these modules have still other unsatisfied deps, they won't start.
 */
static void started_cb(enum modules module) {
    for (int i = 0; i < modules[module].num_dependent; i++) {
        enum modules m = modules[module].dependent_m[i];
        if (!modules[m].disabled) {
            modules[m].self->satisfied_deps++;
            DEBUG("Trying to start %s module as its %s dependency was loaded...\n", modules[m].self->name, modules[module].self->name);
            init_modules(m);
        }
    }
}

/*
 * Calls correct poll cb function;
 * then, if there are modules depending on this module,
 * try to start them calling started_cb.
 */
void poll_cb(const enum modules module) {
    if (modules[module].inited && !modules[module].disabled) {
        if (modules[module].poll_cb) {
            modules[module].poll_cb();
        }
        /* If module has deps, call start cb on them, to start them */
        if (modules[module].dependent_m) {
            started_cb(module);
            free(modules[module].dependent_m);
            modules[module].dependent_m = NULL;
            modules[module].num_dependent = 0;
        }
    }
}

/*
 * Recursively disable a module and all of modules that require it (HARD dep).
 * If a module had a SOFT dep on "module", increment its 
 * satisfied_deps counter and try to init it.
 * Moreover, if "module" is the only dependent module on another module X,
 * disable X too.
 */
void disable_module(const enum modules module) {
    if (!modules[module].disabled) {
        modules[module].disabled = 1;
        INFO("%s module disabled.\n", modules[module].self->name);
        
        /* Cycle to disable all modules dependent on "module", if dep is HARD */
        for (int i = 0; i < modules[module].num_dependent; i++) {
            enum modules m = modules[module].dependent_m[i];
            if (!modules[m].disabled) {
                for (int j = 0; j < modules[m].self->num_deps; j++) {
                    if (modules[m].self->deps[j].dep == module) {
                        if (modules[m].self->deps[j].type == HARD) {
                            DEBUG("Disabling module %s as its hard dep %s was disabled...\n", modules[m].self->name, modules[module].self->name);
                            disable_module(m); // recursive call
                        } else {
                            DEBUG("Trying to start %s module as its %s soft dep was disabled...\n", modules[m].self->name, modules[module].self->name);
                            modules[m].self->satisfied_deps++;
                            init_modules(m);
                        }
                        break;
                    }
                }
            }
        }
        
        /* 
         * Cycle to disable all module on which "module" has a dep,
         * if "module" is the only module dependent on it 
         */
        for (int i = 0; i < modules[module].self->num_deps; i++) {
            const enum modules m = modules[module].self->deps[i].dep;
            /* if module "module" is only dependent module on it, disable it */
            if (modules[m].num_dependent == 1) {
                disable_module(m);
            }
        }
        
        /* Finally, free dependent_m for this disabled module and destroy it */
        if (modules[module].dependent_m) {
            free(modules[module].dependent_m);
            modules[module].dependent_m = NULL;
            modules[module].num_dependent = 0;
        }
        destroy_modules(module);
    }
}

/*
 * Calls correct destroy function for each module
 */
void destroy_modules(const enum modules module) {
    if (modules[module].inited) {
        /* If fd is being polled, close it. Do not close BUS fd!! */
        if (main_p[modules[module].self->idx].fd > 0 && module != BUS) {
            close(main_p[modules[module].self->idx].fd);
        }
        /* call module destroy func */
        modules[module].destroy();
        INFO("%s module destroyed.\n", modules[module].self->name);
        modules[module].inited = 0;
    }
}

