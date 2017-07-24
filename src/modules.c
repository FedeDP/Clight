#include "../inc/modules.h"
#include "../inc/bus.h"

static void started_cb(enum modules module);
static void destroy_module(const enum modules module);
static void disable_module(const enum modules module);

static enum modules *sorted_modules; // modules sorted by their starting place
static int started_modules = 0; // number of started modules

/* 
 * Start a module only if it is not disabled, it is not inited, and a proper init hook function has been setted.
 * Check if all deps modules have been started too.
 * If module has not a poll_cb (it is not waiting on poll), call poll_cb right now as it is fully started already.
 */
void init_modules(const enum modules module) {
    /* if module is not disabled, try to start it */
    if (modules[module].state == UNKN) {
        if (modules[module].self->num_deps == modules[module].self->satisfied_deps) {
            if ((conf.single_capture_mode && !modules[module].self->enabled_single_capture) || modules[module].check()) {
                disable_module(module);
            } else {
                modules[module].init();
            }
        }
    } else if (is_disabled(module)) {
        /* 
         * if module is already disabled it means it was disabled with
         * a --no-X cmdline option/conf file option.
         * Force-disable it
         */
        modules[module].state = UNKN;
        disable_module(module);
    }
}

void init_module(int fd, enum modules module, ...) {
    if (fd == -1) {
        ERROR("%s\n", strerror(errno));
    }
    
    main_p[module] = (struct pollfd) {
        .fd = fd,
        .events = POLLIN,
    };
    
    /* Increment sorted_modules size and store this module in its correct position */
    enum modules *tmp = realloc(sorted_modules, (++started_modules) * sizeof(enum modules));
    if (tmp) {
        sorted_modules = tmp;
        sorted_modules[started_modules - 1] = module;
    } else {
        free(sorted_modules);
        ERROR("%s\n", strerror(errno));
    }
    
    /* 
     * if fd==DONT_POLL_W_ERR, it means a not-critical error happened
     * while module was setting itself up, before calling init_module.
     * eg: geoclue2 support is enabled but geoclue2 could not be found.
     */
    if (fd != DONT_POLL_W_ERR) {
        modules[module].state = INITED;
        DEBUG("%s module started.\n", modules[module].self->name);
        
        /* foreach bus_cb passed in, call add_mod_callback on bus */
        va_list args;
        va_start(args, module);
        struct bus_cb *cb = va_arg(args, struct bus_cb *);
        while (cb) {
            add_mod_callback(*cb);
            DEBUG("Callback added for module %d on module %d bus match.\n", module, cb->module);
            cb = va_arg(args, struct bus_cb *);
        }
        va_end(args);
        
        /* 
         * If module has not an fd (so, it is a oneshot module), 
         * consider this module as started right now.
         */
        if (fd == DONT_POLL) {
            started_cb(module);
        }
        
    } else {
        /* module should be disabled */
        WARN("Error while loading %s module.\n", modules[module].self->name);
        disable_module(module); // disable this module and all of dependent module
    }
}

int is_disabled(enum modules module) {
    return modules[module].state == DISABLED;
}

int is_inited(enum modules module) {
    return modules[module].state == INITED;
}

/*
 * Foreach dep, set self as dependent on that module
 */
void set_self_deps(struct self_t *self) {
    for (int i = 0; i < self->num_deps; i++) {
        enum modules m = self->deps[i].dep;
        modules[m].dependent_m = realloc(modules[m].dependent_m, (++modules[m].num_dependent) * sizeof(enum modules));
        if (!modules[m].dependent_m) {
            ERROR("%s\n", strerror(errno));
            break;
        }
        modules[m].dependent_m[modules[m].num_dependent - 1] = self->idx;
    }
}

/*
 * Callback called when a module that is needed by some others
 * gets started: it increment satisfied_deps for each dependent modules
 * and tries to start them.
 * If these modules have still other unsatisfied deps, they won't start.
 */
static void started_cb(enum modules module) {
    while (modules[module].num_dependent > 0) {
        enum modules m = *modules[module].dependent_m;
        
        if (modules[module].num_dependent > 1) {
            memmove(&modules[module].dependent_m[0], &modules[module].dependent_m[1], (modules[module].num_dependent - 1) * sizeof(enum modules));
        }
        modules[module].dependent_m = realloc(modules[module].dependent_m, (--modules[module].num_dependent) * sizeof(enum modules));
        
        if (modules[m].state == UNKN) {
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
    if (is_inited(module)) {
        if (modules[module].poll_cb) {
            modules[module].poll_cb();
        }
        started_cb(module);
    }
}

void change_dep_type(const enum modules mod, const enum modules mod_dep, const enum dep_type type) {
    for (int i = 0; i < modules[mod].self->num_deps; i++) {
        if (modules[mod].self->deps[i].dep == mod_dep) {
            modules[mod].self->deps[i].type = type;
            break;
        }
    }
}

/*
 * Recursively disable a module and all of modules that require it (HARD dep).
 * If a module had a SOFT dep on "module", increment its 
 * satisfied_deps counter and try to init it.
 * Moreover, if "module" is the only dependent module on another module X,
 * and X is not mandatory for clight, disable X too.
 */
void disable_module(const enum modules module) {
    if (!is_disabled(module)) {
        modules[module].state = DISABLED;
        DEBUG("%s module disabled.\n", modules[module].self->name);

        /* Cycle to disable all modules dependent on "module", if dep is HARD */
        for (int i = 0; i < modules[module].num_dependent; i++) {
            enum modules m = modules[module].dependent_m[i];
            if (!is_disabled(m)) {
                for (int j = 0; j < modules[m].self->num_deps; j++) {
                    if (modules[m].self->deps[j].dep == module) {
                        if (modules[m].self->deps[j].type == HARD) {
                            DEBUG("Disabling module %s as its hard dep %s was disabled...\n", modules[m].self->name, modules[module].self->name);
                            disable_module(m);
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
            for (int j = 0; j < modules[m].num_dependent; j++) {
                if (modules[m].dependent_m[j] == module) {
                    /* properly decrement num_dependent and realloc array of dependent_m */
                    if (j + 1 < modules[m].num_dependent) {
                        memmove(&modules[m].dependent_m[j], &modules[m].dependent_m[j + 1], (modules[m].num_dependent - j - 1) * sizeof(enum modules));
                    }
                    modules[m].dependent_m = realloc(modules[m].dependent_m, (--modules[m].num_dependent) * sizeof(enum modules));
                    
                    /* 
                     * if there are no more dependent_m on this module, 
                     * and it is not a standalone module, disable it
                     */
                    if (modules[m].num_dependent == 0 && !modules[m].self->standalone) {
                        disable_module(m);
                    }
                    break;
                }
            }
        }
        
        /* Finally, destroy this module */
        destroy_module(module);
    }
}

void destroy_modules(void) {
    for (int i = started_modules - 1; i >= 0; i--) {
        destroy_module(sorted_modules[i]);
    }
}

/*
 * Calls correct destroy function for each module
 */
static void destroy_module(const enum modules module) {
    if (modules[module].num_dependent) {
        free(modules[module].dependent_m);
        modules[module].dependent_m = NULL;
        modules[module].num_dependent = 0;
    }
    
    if (is_inited(module)) {
        /* call module destroy func */
        modules[module].destroy();
        /* If fd is being polled, close it. */
        if (main_p[modules[module].self->idx].fd > 0) {
            close(main_p[modules[module].self->idx].fd);
            /* stop polling on this module! */
            main_p[modules[module].self->idx].fd = -1;
        }
        DEBUG("%s module destroyed.\n", modules[module].self->name);
    }
}

