#include "../inc/xorg.h"
#include "../inc/modules.h"

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
    state.display = getenv("DISPLAY");
    state.xauthority = getenv("XAUTHORITY");
    return !state.display || !state.xauthority;
}

static void destroy(void) {
    /* Skeleton function needed for modules interface */
}

static void callback(void) {
    /* Skeleton function needed for modules interface */
}
