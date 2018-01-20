#include "../inc/brightness_smooth.h"
#include "../inc/brightness.h"
#include "../inc/bus.h"

#define BRIGHTNESS_SMOOTH_TIMEOUT 30 * 1000 * 1000 // 30ms

static void init(void);
static int check(void);
static void destroy(void);
static void callback(void);
static void dim_backlight(void);

static struct dependency dependencies[] = { {SUBMODULE, BRIGHTNESS}, {HARD, BUS} };
static struct self_t self = {
    .name = "BrightnessSmooth",
    .idx = BRIGHTNESS_SMOOTH,
    .num_deps = SIZE(dependencies),
    .deps =  dependencies
};
static double target_pct;

void set_brightness_smooth_self(void) {
    SET_SELF();
}

static void init(void) {
    /* Brightness smooth should not start immediately */
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
    dim_backlight();
}

static void dim_backlight(void) {
    double new_pct;
    if (target_pct > state.current_br_pct) {
        new_pct = state.current_br_pct + 0.05; // 5% steps
        set_backlight_level(new_pct > target_pct ? target_pct : new_pct);
    } else {
        new_pct = state.current_br_pct - 0.05;
        set_backlight_level(new_pct < target_pct ? target_pct : new_pct);
    }
    if (state.current_br_pct != target_pct) {
        start_smooth_brightness(BRIGHTNESS_SMOOTH_TIMEOUT, target_pct);
    }
}

void start_smooth_brightness(long delay, double pct) {
    set_timeout(0, delay, main_p[self.idx].fd, 0);
    target_pct = pct;
}

