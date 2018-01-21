#include "../inc/dimmer_smooth.h"
#include "../inc/brightness.h"
#include "../inc/bus.h"

#define DIMMER_SMOOTH_TIMEOUT   30 * 1000 * 1000 // 30ms
#define DIMMER_SMOOTH_STEP      0.05

static void init(void);
static int check(void);
static void destroy(void);
static void callback(void);
static void dim_backlight(void);
static void restore_backlight(void);

static struct dependency dependencies[] = { {SUBMODULE, DIMMER}, {HARD, BRIGHTNESS}, {HARD, BUS}, {HARD, XORG} };
static struct self_t self = {
    .name = "DimmerSmooth",
    .idx = DIMMER_SMOOTH,
    .num_deps = SIZE(dependencies),
    .deps =  dependencies
};
static double target_pct;

void set_dimmer_smooth_self(void) {
    SET_SELF();
}

static void init(void) {
    /* Dimmer smooth should not start immediately */
    int fd = start_timer(CLOCK_MONOTONIC, 0, 0);
    INIT_MOD(fd);
}

static int check(void) {
    return 0;
}

static void destroy(void) {
    /* Skeleton function needed for modules interface */
}

static void callback(void) {
    uint64_t t;
    read(main_p[self.idx].fd, &t, sizeof(uint64_t));
    
    if (state.is_dimmed) {
        dim_backlight();
    } else {
        restore_backlight();
    }
}

static void dim_backlight(void) {
    const double lower_br_pct = state.current_br_pct - DIMMER_SMOOTH_STEP; // 5% steps
    set_backlight_level(lower_br_pct > target_pct ? lower_br_pct : target_pct);
    if (state.current_br_pct != target_pct) {
        start_dimmer_smooth(DIMMER_SMOOTH_TIMEOUT, target_pct);
    }
}

static void restore_backlight(void) {
    const double new_br_pct = state.current_br_pct + DIMMER_SMOOTH_STEP; // 5% steps
    set_backlight_level(new_br_pct > target_pct ? target_pct : new_br_pct);
    if (state.current_br_pct != target_pct) {
        start_dimmer_smooth(DIMMER_SMOOTH_TIMEOUT, target_pct);
    }
}

void start_dimmer_smooth(long delay, double pct) {
    set_timeout(0, delay, main_p[self.idx].fd, 0);
    target_pct = pct;
}
