#include "../inc/dimmer_smooth.h"
#include "../inc/brightness.h"

#define DIMMER_SMOOTH_TIMEOUT 30 * 1000 * 1000 // 30ms

static void init(void);
static int check(void);
static void destroy(void);
static void callback(void);
static void dim_backlight(void);
static void restore_backlight(void);

static int dimmed_br;
static struct dependency dependencies[] = { {HARD, DIMMER}, {HARD, BRIGHTNESS}, {HARD, BUS}, {HARD, XORG} };
static struct self_t self = {
    .name = "DimmerSmooth",
    .idx = DIMMER_SMOOTH,
    .num_deps = SIZE(dependencies),
    .deps =  dependencies
};

void set_dimmer_smooth_self(void) {
    SET_SELF();
}

static void init(void) {
    /* Dimmer smooth should not start immediately */
    int fd = start_timer(CLOCK_MONOTONIC, 0, 0);
    init_module(fd, self.idx, NULL);
    /* brightness module is started before dimmer, so state.br.max is already ok there */
    dimmed_br = (double)state.br.max * conf.dimmer_pct / 100;
}

/* Check we're on X, dimmer is enabled and smooth transitioning are enabled */
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
    const int lower_br = (double)state.br.current - (double)state.br.max / 20; // 5% steps
    set_backlight_level(lower_br > dimmed_br ? lower_br : dimmed_br);
    if (state.br.current != dimmed_br) {
        start_smooth_transition(DIMMER_SMOOTH_TIMEOUT);
    }
}

static void restore_backlight(void) {
    const int new_br = (double)state.br.current + (double)state.br.max / 20; // 5% steps
    set_backlight_level(new_br > state.br.old ? state.br.old : new_br);
    if (state.br.current != state.br.old) {
        start_smooth_transition(DIMMER_SMOOTH_TIMEOUT);
    }
}

void start_smooth_transition(long delay) {
    set_timeout(0, delay, main_p[self.idx].fd, 0);
}
