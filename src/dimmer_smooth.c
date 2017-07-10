#include "../inc/dimmer_smooth.h"
#include "../inc/brightness.h"

#define DIMMER_SMOOTH_TIMEOUT 30 * 1000 * 1000 // 30ms

static void init(void);
static int check(void);
static void destroy(void);
static void dimmer_smooth_cb(void);
static void dim_backlight(void);

static int dimmed_br;
static struct dependency dependencies[] = { {HARD, DIMMER}, {HARD, BRIGHTNESS}, {HARD, BUS} };
static struct self_t self = {
    .name = "DimmerSmooth",
    .idx = DIMMER_SMOOTH,
    .num_deps = SIZE(dependencies),
    .deps =  dependencies
};

void set_dimmer_smooth_self(void) {
    modules[self.idx].self = &self;
    modules[self.idx].init = init;
    modules[self.idx].check = check;
    modules[self.idx].destroy = destroy;
    set_self_deps(&self);
}

static void init(void) {
    int fd = start_timer(CLOCK_MONOTONIC, 0, 0);
    init_module(fd, self.idx, dimmer_smooth_cb);
    /* brightness module is started before dimmer, so state.br.max is already ok there */
    dimmed_br = (double)state.br.max * conf.dimmer_pct / 100;
}

/* Check we're on X, dimmer is enabled and smooth transitioning are enabled */
static int check(void) {
    return  conf.single_capture_mode || conf.no_dimmer || !state.display 
    || !state.xauthority || conf.no_dimmer_smooth_transition;
}

static void destroy(void) {
    /* Skeleton function needed for modules interface */
}

static void dimmer_smooth_cb(void) {
    uint64_t t;
    read(main_p[self.idx].fd, &t, sizeof(uint64_t));
    
    dim_backlight();
}

static void dim_backlight(void) {
    const int lower_br = (double)state.br.current - (double)state.br.max / 20; // 5% steps
    set_backlight_level(lower_br > dimmed_br ? lower_br : dimmed_br);
    if (state.br.current != dimmed_br) {
        start_smooth_transition();
    }
}

void start_smooth_transition(void) {
    set_timeout(0, DIMMER_SMOOTH_TIMEOUT, main_p[self.idx].fd, 0);
}

void stop_smooth_transition(void) {
    set_timeout(0, 0, main_p[self.idx].fd, 0);
}
