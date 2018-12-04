#include <bus.h>
#include <my_math.h>
#include <interface.h>

static void check_gamma(void);
static void get_gamma_events(const time_t *now, const float lat, const float lon, int day);
static void check_next_event(const time_t *now);
static void check_state(const time_t *now);
static void set_temp(int temp);
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

static enum events next_event;                 // next event index (sunrise/sunset)
static int event_time_range;                   // variable that holds minutes in advance/after an event to enter/leave EVENT state

MODULE(GAMMA);

static void init(void) {
    struct bus_cb loc_cb = { LOCATION, location_callback };
    struct bus_cb interface_cb = { INTERFACE, interface_callback, "gamma" };

    int fd = start_timer(CLOCK_BOOTTIME, 0, 1);
    INIT_MOD(fd, &loc_cb, &interface_cb);
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
    check_gamma();
}

static void check_gamma(void) {
    static int first_time = 1;

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
    set_temp(conf.temp[state.time]);

    /* desired gamma temp has been set. Set new GAMMA timer and reset transitioning state. */
    time_t next = state.events[next_event] + event_time_range;
    INFO("Next gamma alarm due to: %s", ctime(&next));
    set_timeout(next - t, 0, main_p[self.idx].fd, 0);

    /* 
     * if we entered/left an event, emit PropertiesChanged signal
     * Avoid spamming signal firt time we start (as we're computing current time)
     * eg: we start clight during the NIGHT but state.time defaults to 0 (ie: DAY).
     */
    if (!first_time) {
        if (old_state != state.time) {
            emit_prop("Time");
        }
    } else {
        first_time = 0;
    }
}

/*
 * day -> will be 0 first time this func is called, else 1 (tomorrow).
 * Stores day sunrise/sunset events only if this is first time it is called,
 * or of today's sunset event is finished.
 * Firstly computes day's sunrise and sunset; then calls check_next_event and check_state to
 * update global state.next_event and state.time variables according to new state.
 * Note that "+1" is because it seems timerfd receives timer end circa 1s in advance.
 * Probably it is just some ms in advance, but rounding it to seconds returns 1s in advance.
 */
static void get_gamma_events(const time_t *now, const float lat, const float lon, int day) {
    time_t t;

    /* only every new day, after latest event of today finished */
    if (*now + 1 >= state.events[SUNSET] + conf.event_duration) {
        if (calculate_sunset(lat, lon, &t, day) == 0) {
            if (*now + 1 >= t + conf.event_duration) {
                /*
                 * we're between today's sunset and tomorrow sunrise.
                 * rerun function with tomorrow.
                 * Useful only first time clight is started
                 * (ie: if it is started at night time)
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
    check_next_event(now);
    check_state(now);
}

/*
 * Updates state.next_event global var, according to now time_t value.
 * Note that "+1" is because it seems timerfd receives timer end circa 1s in advance.
 */
static void check_next_event(const time_t *now) {
    if (*now + 1 < state.events[SUNRISE] + conf.event_duration || state.events[SUNSET] == -1) {
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
    if (labs(state.events[next_event] - (*now + 1)) <= conf.event_duration) {
        int event_t;

        if (state.events[next_event] > *now + 1) {
            event_t = next_event;
            event_time_range = 0;
        } else {
            event_t = !next_event;
            event_time_range = conf.event_duration;
        }
        conf.temp[EVENT] = event_t == SUNRISE ? conf.temp[NIGHT] : conf.temp[DAY];
        state.time = EVENT;
        DEBUG("Currently inside an event.\n");
    } else {
        state.time = next_event == SUNRISE ? NIGHT : DAY;
        event_time_range = -conf.event_duration; // 30mins before event
    }
}

static void set_temp(int temp) {
    int ok;

    SYSBUS_ARG(args, CLIGHTD_SERVICE, "/org/clightd/clightd/Gamma", "org.clightd.clightd.Gamma", "Set");
    int r = call(&ok, "b", &args, "ssi(buu)", state.display, state.xauthority, temp, !conf.no_smooth_gamma, conf.gamma_trans_step, conf.gamma_trans_timeout);
    if (!r && ok) {
        INFO("%d gamma temp set.\n", temp);
    }
}

static void location_callback(const void *ptr) {
    struct location *old_loc = (struct location *)ptr;
    /* Check if new position is at least 20kms distant */
    if (get_distance(old_loc, &state.current_loc) > LOC_DISTANCE_THRS) {
        /* Updated GAMMA module sunrise/sunset for new location */
        state.events[SUNSET] = 0; // to force get_gamma_events to recheck sunrise and sunset for today
        set_timeout(0, 1, main_p[self.idx].fd, 0);
    }
}

static void interface_callback(const void *ptr) {
    time_t t = time(NULL);
    check_state(&t); // update conf.temp in case we're during an EVENT
    set_temp(conf.temp[state.time]);
}
