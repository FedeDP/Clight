#include <modules.h>
#include <interface.h>
#include <bus.h>

static void init_submodules(const enum modules module);
static void started_cb(enum modules module);
static void set_state(struct module *mod, const enum module_states state);
static void destroy_module(const enum modules module);
static void disable_module(const enum modules module);

static enum modules *sorted_modules; // modules sorted by their starting place
static int started_modules = 0; // number of started modules

/* 
 * Start a module only if it is not disabled, it is not inited, and a proper init hook function has been set.
 * Check if all deps modules have been started too.
 * If module has not a poll_cb (it is not waiting on poll), call poll_cb right now as it is fully started already.
 */
void init_modules(const enum modules module) {
    /* if module is not disabled, try to start it */
    if (is_idle(module)) {
        if (modules[module].self->num_deps == modules[module].self->satisfied_deps) {
            if (modules[module].check()) {
                disable_module(module);
            } else {
                modules[module].init();
            }
        }
    } else if (is_started_disabled(module)) {
        disable_module(module);
    }
}

void init_module(int fd, enum modules module, ...) {
    if (fd == -1) {
        ERROR("%s\n", strerror(errno));
    }

    /* When no_auto_calib is true, functional modules are all started paused */
    if (modules[module].self->functional_module && conf.no_auto_calib && fd >= 0) {
        close(fd);
        fd = DONT_POLL;
    }

    main_p[module] = (struct pollfd) {
        .fd = fd,
        .events = POLLIN,
    };

    /* 
     * if fd==DONT_POLL_W_ERR, it means a not-critical error happened
     * while module was setting itself up, before calling init_module.
     * eg: geoclue2 support is enabled but geoclue2 could not be found.
     */
    if (fd != DONT_POLL_W_ERR) {
        /* Increment sorted_modules size and store this module in its correct position */
        enum modules *tmp = realloc(sorted_modules, (++started_modules) * sizeof(enum modules));
        if (tmp) {
            sorted_modules = tmp;
            sorted_modules[started_modules - 1] = module;
        } else {
            free(sorted_modules);
            ERROR("%s\n", strerror(errno));
        }

        set_state(&modules[module], RUNNING);

        DEBUG("%s module started.\n", modules[module].self->name);

        /* foreach bus_cb passed in, call add_mod_callback on bus */
        va_list args;
        va_start(args, module);
        struct bus_cb *cb = va_arg(args, struct bus_cb *);
        while (cb) {
            if (is_running(cb->module)) {
                add_mod_callback(cb);
                DEBUG("Callback added for module %s on module %s bus match.\n", modules[module].self->name, modules[cb->module].self->name);
            }
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

        init_submodules(module);

    } else {
        /* module should be disabled */
        WARN("Error while loading %s module.\n", modules[module].self->name);
        disable_module(module); // disable this module and all of dependent module
    }
}

static void init_submodules(const enum modules module) {
    for (int i = 0; i < MODULES_NUM; i++) {
        const enum dep_type type = modules[module].dependent_m[i];
        if (type == SUBMODULE) {
            if (is_idle(i)) {
                DEBUG("%s module being started as submodule of %s...\n", modules[i].self->name, modules[module].self->name);
                modules[i].self->satisfied_deps++;
                init_modules(i);
            }
        }
    }
}

int is_disabled(const enum modules module) {
    return modules[module].state == DISABLED;
}

int is_running(const enum modules module) {
    return modules[module].state == RUNNING;
}

int is_started_disabled(const enum modules module) {
    return modules[module].state == STARTED_DISABLED;
}

int is_idle(const enum modules module) {
    return modules[module].state == IDLE;
}

int is_destroyed(const enum modules module) {
    return modules[module].state == DESTROYED;
}

/*
 * Foreach dep, set self as dependent on that module
 */
void set_self_deps(struct self_t *self) {
    for (int i = 0; i < self->num_deps; i++) {
        enum modules m = self->deps[i].dep;
        const enum dep_type type = self->deps[i].type;
        modules[m].dependent_m[self->idx] = type;
        modules[m].num_dependent++;
    }
}

/*
 * Callback called when a module that is needed by some others
 * gets started: it increment satisfied_deps for each dependent modules
 * and tries to start them.
 * If these modules have still other unsatisfied deps, they won't start.
 */
static void started_cb(enum modules module) {
    for (int i = 0; i < MODULES_NUM; i++) {
        const enum dep_type type = modules[module].dependent_m[i];
        if (type != NO_DEP && is_idle(i)) {
            modules[i].self->satisfied_deps++;
            DEBUG("Trying to start %s module as its %s dependency was loaded...\n", modules[i].self->name, modules[module].self->name);
            init_modules(i);
        }
    }
}

static void set_state(struct module *mod, const enum module_states state) {
    mod->state = state;
    emit_prop(mod->self->name);
}

/*
 * Calls correct poll cb function;
 * then, if there are modules depending on this module,
 * try to start them calling started_cb.
 */
void poll_cb(const enum modules module) {
    if (is_running(module)) {
        if (modules[module].poll_cb) {
            modules[module].poll_cb();
        }
        started_cb(module);
    }
}

void change_dep_type(const enum modules mod, const enum modules mod_dep, const enum dep_type type) {
    /* update type of dependency in mod deps */
    for (int i = 0; i < modules[mod].self->num_deps; i++) {
        if (modules[mod].self->deps[i].dep == mod_dep) {
            modules[mod].self->deps[i].type = type;
            break;
        }
    }

    /* Update dep type in dependent_m too */
    modules[mod_dep].dependent_m[mod] = type;
}

/*
 * Recursively disable a module and all of modules that require it (HARD dep).
 * If a module had a SOFT dep on "module", increment its 
 * satisfied_deps counter and try to init it.
 * Moreover, if "module" is the only dependent module on another module X,
 * and X is not mandatory for clight, disable X too.
 */
void disable_module(const enum modules module) {
    if (!is_disabled(module) && !is_destroyed(module)) {
        set_state(&modules[module], DISABLED);
        DEBUG("%s module disabled.\n", modules[module].self->name);

        /* Cycle to disable all modules dependent on "module", if dep is HARD */
        for (int i = 0; i < MODULES_NUM; i++) {
            const enum dep_type type = modules[module].dependent_m[i];
            if (type == NO_DEP) {
                continue;
            }
            if (!is_disabled(i) && !is_destroyed(i)) {
                if (type != SOFT) {
                    DEBUG("Disabling module %s as its hard dep %s was disabled...\n", modules[i].self->name, modules[module].self->name);
                    disable_module(i);
                } else {
                    DEBUG("Trying to start %s module as its %s soft dep was disabled...\n", modules[i].self->name, modules[module].self->name);
                    modules[i].self->satisfied_deps++;
                    init_modules(i);
                }
            }
        }

        /* 
         * Cycle to disable all module on which "module" has a dep,
         * if "module" is the only module dependent on it 
         */
        for (int i = 0; i < modules[module].self->num_deps; i++) {
            const enum modules m = modules[module].self->deps[i].dep;
            /* 
             * if there are no more dependent_m on this module, 
             * and it is not a standalone module neither a functional module, disable it
             */
            if (--modules[m].num_dependent == 0 && !modules[m].self->standalone && !modules[m].self->functional_module) {
                disable_module(m);
            }
        }

        /* Update the list of active functional modules */
        state.needed_functional_modules -= modules[module].self->functional_module;
        if (state.needed_functional_modules == 0) {
            ERROR("No functional module is running. Nothing to do...leaving.\n");
        }
        /* Finally, destroy this module */
        destroy_module(module);
    }
}

void destroy_modules(void) {
    for (int i = started_modules - 1; i >= 0; i--) {
        destroy_module(sorted_modules[i]);
    }
    free(sorted_modules);
}

/*
 * Calls correct destroy function for each module
 */
static void destroy_module(const enum modules module) {
    if (!is_destroyed(module)) {
        /* call module destroy func */
        modules[module].destroy();
        /* If fd is being polled, close it. */
        if (main_p[module].fd > 0) {
            close(main_p[module].fd);
            /* stop polling on this module! */
            main_p[module].fd = -1;
        }
        set_state(&modules[module], DESTROYED);
        DEBUG("%s module destroyed.\n", modules[module].self->name);
    }
}

