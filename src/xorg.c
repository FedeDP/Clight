#include "../inc/xorg.h"
#include "../inc/modules.h"
#include <systemd/sd-login.h>

static void init(void);
static int check(void);
static void destroy(void);
static void callback(void);

static struct self_t self = {
    .name = "Xorg",
    .idx = XORG,
    .standalone = 1
};

void set_xorg_self(void) {
    SET_SELF();
}

static void init(void) {
    INIT_MOD(DONT_POLL);
}

/* Check we're on X */
static int check(void) {
    sd_session_get_display(NULL, &state.display);
    if (getenv("XAUTHORITY")) {
        state.xauthority = strdup(getenv("XAUTHORITY"));
    }
    if (!state.xauthority) {
        char *sess_type = NULL;
        if (sd_session_get_type(NULL, &sess_type) >= 0) {
            if (!strncmp(sess_type, "x11", strlen("x11"))) {
                char default_path[PATH_MAX + 1] = {0};
                snprintf(default_path, PATH_MAX, "%s/.Xauthority", getpwuid(getuid())->pw_dir);
                if (access(default_path, F_OK) != -1) {
                    state.xauthority = strdup(default_path);
                }
            }
            free(sess_type);
        }
    }
    return !state.display || !state.xauthority;
}

static void destroy(void) {
    free(state.display);
    free(state.xauthority);
}

static void callback(void) {
    /* Skeleton function needed for modules interface */
}


