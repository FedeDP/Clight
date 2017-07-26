#include "../inc/gamma_smooth.h"

#define GAMMA_SMOOTH_TIMEOUT 300 * 1000 * 1000  // 300 ms

static void init(void);
static int check(void);
static void destroy(void);
static void callback(void);

static struct dependency dependencies[] = { {HARD, GAMMA}, {HARD, BUS}, {HARD, XORG} };
static struct self_t self = {
    .name = "GammaSmooth",
    .idx = GAMMA_SMOOTH,
    .num_deps = SIZE(dependencies),
    .deps =  dependencies
};

void set_gamma_smooth_self(void) {
    SET_SELF();
}

static void init(void) {
    /* 
     * gamma smooth must smooth transitionins
     * as soon as it is started
     * to set correct gamma for current time. 1ns timeout
     */
    int fd = start_timer(CLOCK_MONOTONIC, 0, 1);
    INIT_MOD(fd);
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
    
    if (set_temp(conf.temp[state.time])) {
        start_gamma_transition(GAMMA_SMOOTH_TIMEOUT);
    }
}

void start_gamma_transition(long delay) {
    set_timeout(0, delay, main_p[self.idx].fd, 0);
}

/*
 * First, it gets current gamma value.
 * Then, if current value is != from temp, it will adjust screen temperature accordingly.
 * If smooth_transition is enabled, the function will return 1 until they are the same.
 * old_temp is static so we don't have to call getgamma everytime the function is called (if smooth_transition is enabled.)
 * and gets resetted when old_temp reaches correct temp.
 */
int set_temp(int temp) {
    const int step = 50;
    int new_temp = -1;
    static int old_temp = 0;
    
    if (old_temp == 0) {
        struct bus_args args_get = {"org.clightd.backlight", "/org/clightd/backlight", "org.clightd.backlight", "getgamma"};
        
        call(&old_temp, "i", &args_get, "ss", state.display, state.xauthority);
    }
    
    if (old_temp != temp) {
        struct bus_args args_set = {"org.clightd.backlight", "/org/clightd/backlight", "org.clightd.backlight", "setgamma"};
        
        if (is_inited(self.idx)) {
            if (old_temp > temp) {
                old_temp = old_temp - step < temp ? temp : old_temp - step;
            } else {
                old_temp = old_temp + step > temp ? temp : old_temp + step;
            }
            call(&new_temp, "i", &args_set, "ssi", state.display, state.xauthority, old_temp);
        } else {
            call(&new_temp, "i", &args_set, "ssi", state.display, state.xauthority, temp);
        }
        if (new_temp == temp) {
            // reset old_temp for next call
            old_temp = 0;
            INFO("%d gamma temp setted.\n", temp);
        }
    } else {
        // reset old_temp
        old_temp = 0;
        new_temp = temp;
        INFO("Gamma temp was already %d\n", temp);
    }
    return new_temp != temp;
}
