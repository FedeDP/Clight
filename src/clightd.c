#include "../inc/clightd.h"
#include "../inc/bus.h"

static void init(void);
static int check(void);
static void destroy(void);
static void callback(void);
static int check_clightd_version(void);

#define MINIMUM_CLIGHTD_VERSION_MAJ 1
#define MINIMUM_CLIGHTD_VERSION_MIN 5

static struct self_t self = {
    .name = "Clightd",
    .idx = CLIGHTD,
    .standalone = 1,
    .enabled_single_capture = 1
};

void set_clightd_self(void) {
    SET_SELF();
}

static void init(void) {
    INIT_MOD(DONT_POLL);
}

static int check(void) {
    return check_clightd_version();
}

static void destroy(void) {
    state.quit = ERR_QUIT;
}

static void callback(void) {
    /* Skeleton function needed for modules interface */
}

static int check_clightd_version(void) {
    int ret = -1;
    const char *clightd_version = NULL;
    struct bus_args args = {"org.clightd.backlight", "/org/clightd/backlight", "org.clightd.backlight", "version"};
    get_property(&args, "s", &clightd_version);

    if (!clightd_version) {
        WARN("No clightd found. Clightd is a mandatory dep.\n");
    } else {
        int maj_val = atoi(clightd_version);
        int min_val = atoi(strchr(clightd_version, '.') + 1);
        if (maj_val < MINIMUM_CLIGHTD_VERSION_MAJ || (maj_val == MINIMUM_CLIGHTD_VERSION_MAJ && min_val < MINIMUM_CLIGHTD_VERSION_MIN)) {
            WARN("Clightd must be updated. Required version: %d.%d.\n", MINIMUM_CLIGHTD_VERSION_MAJ, MINIMUM_CLIGHTD_VERSION_MIN);
        } else {
            INFO("Clightd found, version: %s.\n", clightd_version);
            ret = 0;
        }
    }
    return ret;
}
