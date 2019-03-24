#include <bus.h>
#include <my_math.h>
#include <interface.h>

static void check_gamma(void);
static void get_gamma_events(const time_t *now, const float lat, const float lon, int day);
static void check_next_event(const time_t *now, int day);
static void check_state(const time_t *now);
static void set_temp(int temp, const time_t *now);
static void location_callback(const void *ptr);
static void interface_callback(const void *ptr);

static struct dependency dependencies[] = {
    {HARD, LOCATION},   // Needed for sunrise/sunset computation
    {HARD, XORG},       // This module is xorg only
    {HARD, CLIGHTD},    // We need clightd
    {SOFT, INTERFACE}   // It adds a callback on INTERFACE
};
static struct self_t self = {
    .num_deps = SIZE(dependencies),
    .deps =  dependencies,
    .functional_module = 1
};

static enum events next_event;                 // next event index (sunrise/sunset) -> NOTE that it will point to current event until conf.event_duration is elapsed.
static int event_time_range;                   // variable that holds minutes in advance/after an event to enter/leave EVENT state
static int long_transitioning;                 // are we inside a long transition?

MODULE(GAMMA);

static void init(void) {
    struct bus_cb loc_cb = { LOCATION, location_callback };
    struct bus_cb interface_cb = { INTERFACE, interface_callback, "gamma" };

    /* Initial value */
    state.time = -1;
    
    int fd = start_timer(CLOCK_BOOTTIME, 0, 1);
    INIT_MOD(fd, &loc_cb, &interface_cb);
}

static int check(void) {
    return 0;
}

static void destroy(void) {
    /* Skeleton function needed for modules interface */
}

static int callback(void) {
    uint64_t t;

    read(main_p[self.idx].fd, &t, sizeof(uint64_t));
    check_gamma();
    return 0;
}

static void check_gamma(void) {
    const time_t t = time(NULL);
    /*
     * get_gamma_events will always poll today events. It should not be necessary,
     * (as it will normally only be needed to get new events for tomorrow, once clight is started)
     * but if, for example, laptop gets suspended at evening, before computing next day events,
     * and it is waken next morning, it will proceed to compute "tomorrow" events, where tomorrow is
     * the wrong day (it should compute "today" events). Thus, avoid this kind of issue.
     */
    enum states old_state = state.time;
    get_gamma_events(&t, state.current_loc.lat, state.current_loc.lon, 0);

    /*
     * Force set every time correct gamma
     * to avoid any possible sync issue
     * between time of day and gamma (eg after long suspend).
     * Note that in case correct gamma temp is already set,
     * it won't do anything.
     */
    set_temp(conf.temp[state.time], &t);

    /* desired gamma temp has been set. Set new GAMMA timer and reset transitioning state. */
    time_t next = state.events[next_event] + event_time_range;
    INFO("Next gamma alarm due to: %s", ctime(&next));
    set_timeout(next - t, 0, main_p[self.idx].fd, 0);

    /*
     * if we entered/left an event, emit PropertiesChanged signal
     */
    if (old_state != state.time) {
        emit_prop("Time");
    }
}

/*
 * day -> will be 0 first time this func is called, else 1 (tomorrow).
 * Stores day sunrise/sunset events only if this is first time it is called,
 * or of today's sunset event is finished.
 * Firstly computes day's sunrise and sunset; then calls check_next_event and check_state to
 * update next_event and state.time variables according to new state.
 * Note that "+1" is because it seems timerfd receives timer end circa 1s in advance.
 * Probably it is just some ms in advance, but rounding it to seconds returns 1s in advance.
 */
static void get_gamma_events(const time_t *now, const float lat, const float lon, int day) {
    time_t t;

    /* only every new day, after today's last event (ie: sunset + event_duration) */
    if (*now + 1 >= state.events[SUNSET] + conf.event_duration) {
        if (calculate_sunset(lat, lon, &t, day) == 0) {
            /* If today's sunset was before now, compute tomorrow */
            if (*now + 1 >= t + conf.event_duration) {
                /*
                 * we're between today's sunset and tomorrow sunrise.
                 * rerun function with tomorrow.
                 */
                return get_gamma_events(now, lat, lon, ++day);
            }
            state.events[SUNSET] = t;
            emit_prop("Sunset");
        } else {
            state.events[SUNSET] = -1;
        }

        if (calculate_sunrise(lat, lon, &t, day) == 0) {
            /*
             * Force computation of today event if SUNRISE is
             * not today; eg: in local time it is at 6am, but utc time is 22,
             * so it counts as today while it is indeed tomorrow...
             */
            if (t > state.events[SUNSET]) {
                calculate_sunrise(lat, lon, &t, day - 1);
            }
            state.events[SUNRISE] = t;
            emit_prop("Sunrise");
        } else {
            state.events[SUNRISE] = -1;
        }

        if (state.events[SUNRISE] == -1 && state.events[SUNSET] == -1) {
            /*
             * no sunrise/sunset could be found.
             * Assume day and set sunset 12hrs from now
             */
            state.events[SUNSET] = *now + 12 * 60 * 60;
            WARN("Failed to retrieve sunrise/sunset informations.\n");
        }
    }
    check_next_event(now, day);
    check_state(now);
}

/*
 * Updates next_event global var, according to now time_t value.
 * Note that "+1" is because it seems timerfd receives timer end circa 1s in advance.
 */
static void check_next_event(const time_t *now, int day) {
    /*
     * SUNRISE if:
     * we have just computed tomorrow events
     * We're after state.events[SUNSET] (when clight is started between SUNSET and conf.event_duration)
     * We're before state.events[SUNRISE] (when clight is started before today's SUNRISE + conf.event_duration)
     */
    if (day == 1 
        || *now + 1 > state.events[SUNSET] + conf.event_duration
        || *now + 1 < state.events[SUNRISE] + conf.event_duration) {
        
        next_event = SUNRISE;
    } else {
        next_event = SUNSET;
    }
}

/*
 * Updates state.time global var, according to now time_t value.
 * Note that "+1" is because it seems timerfd receives timer end circa 1s in advance.
 * If we're inside an event, checks which side of the events we're in
 * (to understand which conf.temp is correct for this state).
 * Then sets event_time_range accordingly; ie: 30mins before event, if we're not inside an event;
 * 0 if we just entered an event (so next_event has to be exactly event time, to set new temp),
 * 30mins after event to remove EVENT state.
 */
static void check_state(const time_t *now) {
    int old_in_event = state.in_event;
    if (labs(state.events[next_event] - (*now + 1)) <= conf.event_duration) {
        if (state.events[next_event] > *now + 1) {
            event_time_range = 0; // next timer is on next_event
        } else {
            event_time_range = conf.event_duration; // next timer is when leaving event
        }
        state.in_event = 1;
        DEBUG("Currently inside an event.\n");
    } else {
        event_time_range = -conf.event_duration; // next timer is entering next event
        state.in_event = 0;
    }
    state.time = next_event == SUNRISE ? NIGHT : DAY;
    
    /* 
     * If we're between {EVENT, EVENT + conf.event_duration} 
     * we need to set next event gamma temp
     */
    if (event_time_range == conf.event_duration) {
        state.time = !state.time;
    }
    if (old_in_event != state.in_event) {
        emit_prop("InEvent");
    }
}

static void set_temp(int temp, const time_t *now) {
    /* 
     * Check if we finished a long transition, only when called by GAMMA.
     * When called by bus interface (now = NULL), set desired gamma temp right now.
     */
    if (long_transitioning && now) {
        if (!state.in_event) {
            /* We finished a transition! */
            long_transitioning = 0;
        }
        return;
    }
    
    int ok, smooth, step, timeout;
    
    SYSBUS_ARG(args, CLIGHTD_SERVICE, "/org/clightd/clightd/Gamma", "org.clightd.clightd.Gamma", "Set");
    
    /* Compute long transition steps and timeouts (if outside of event, fallback to normal transition) */
    if (conf.gamma_long_transition && state.in_event && now) {
        smooth = 1;
        if (event_time_range == 0) {
            /* Remaining time in first half + second half of transition */
            timeout = (state.events[next_event] - *now) + conf.event_duration;
            temp = conf.temp[!state.time]; // use correct temp
        } else {
            /* Remaining time in second half of transition */
            timeout = conf.event_duration - (*now - state.events[next_event]);
        }
        /* Temperature difference */
        step = abs(conf.temp[DAY] - conf.temp[NIGHT]);        
        /* Compute each step size with a gamma_trans_timeout of 10s */
        step /= (((double)timeout) / GAMMA_LONG_TRANS_TIMEOUT);
        /* force gamma_trans_timeout to 10s (in ms) */
        timeout = GAMMA_LONG_TRANS_TIMEOUT * 1000;
        
        long_transitioning = 1;
    } else {
        smooth = !conf.no_smooth_gamma;
        step = conf.gamma_trans_step;
        timeout = conf.gamma_trans_timeout;
        
        long_transitioning = 0;
    }
    
    int r = call(&ok, "b", &args, "ssi(buu)", state.display, state.xauthority, temp, smooth, step, timeout);
    if (!r && ok) {
        if (!long_transitioning && conf.no_smooth_gamma) {
            INFO("%d gamma temp set.\n", temp);
        } else {
            INFO("%s transition to %d gamma temp started.\n", long_transitioning ? "Long" : "Normal", temp);
        }
    }
}

static void location_callback(const void *ptr) {
    struct location *old_loc = (struct location *)ptr;
    /* Check if new position is at least 20kms distant */
    if (get_distance(old_loc, &state.current_loc) > LOC_DISTANCE_THRS) {
        /* Updated GAMMA module sunrise/sunset for new location */
        state.events[SUNSET] = 0; // to force get_gamma_events to recheck sunrise and sunset for today
        set_timeout(0, 1, main_p[self.idx].fd, 0);
    } else {
        INFO("New location is close to old one. Skip updating sunrise/sunset times.\n");
    }
}

static void interface_callback(const void *ptr) {
    time_t t = time(NULL);
    check_state(&t); // update conf.temp in case we're during an EVENT
    set_temp(conf.temp[state.time], NULL); // force a refresh (passing NULL time_t*)
}
