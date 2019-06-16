#include <bus.h>

static int check_clightd_version(void);

#define MINIMUM_CLIGHTD_VERSION_MAJ 4
#define MINIMUM_CLIGHTD_VERSION_MIN 0

static struct dependency dependencies[] = {
    {HARD, BUS}     // We need bus to be started before attempt to check for Clightd
};
static struct self_t self = {
    .num_deps = SIZE(dependencies),
    .deps =  dependencies,
};

MODULE("CLIGHTD");

static void init(void) {
    INIT_MOD(DONT_POLL);
}

static bool check(void) {
    return true;
}

static bool evaluate(void) {
//     return check_clightd_version();
    // FIXME!
    return true;
}


static void destroy(void) {
    /* Skeleton function needed for modules interface */
}

static void receive(const msg_t *const msg, const void* userdata) {
    
}

static int check_clightd_version(void) {
    int ret = -1;
    char clightd_version[PATH_MAX + 1] = {0};
    SYSBUS_ARG(args, CLIGHTD_SERVICE, "/org/clightd/clightd", "org.clightd.clightd", "Version");
    
    int r = get_property(&args, "s", clightd_version);
    if (r < 0 || !strlen(clightd_version)) {
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
