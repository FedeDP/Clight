#include <my_math.h>
#include <bus.h>

#define GAMMA_LONG_TRANS_TIMEOUT 10         // 10s between each step with slow transitioning

static void check_gamma(void);
static void get_gamma_events(const time_t *now, const float lat, const float lon, int day);
static void check_next_event(const time_t *now);
static void check_state(const time_t *now);
static void set_temp(int temp, const time_t *now);
static void ambient_callback(void);
static void location_callback(void);
static void interface_callback(int value);

static enum events target_event;               // which event are we targeting?
static time_t last_t;                          // last time_t check_gamma() was called
static int event_time_range;                   // variable that holds minutes in advance/after an event to enter/leave EVENT state
static int long_transitioning;                 // are we inside a long transition?
static int gamma_fd;

static time_upd time_msg = { TIME_UPD };
static time_upd in_ev_msg = { EVENT_UPD };
static evt_upd evt_msg[SIZE_EVENTS] = { { SUNRISE_UPD }, { SUNSET_UPD } };
static temp_upd temp_msg = { TEMP_UPD };

MODULE("GAMMA");

static void init(void) {
    M_SUB(CURRENT_BL_UPD);
    M_SUB(LOCATION_UPD);
    M_SUB(TEMP_REQ);
    
    gamma_fd = start_timer(CLOCK_BOOTTIME, 0, 1);
    m_register_fd(gamma_fd, true, NULL);
}

static bool check(void) {
    return state.display && state.xauthority;
}

static bool evaluate(void) {
    return !conf.no_gamma && 
           ((state.current_loc.lat != LAT_UNDEFINED && state.current_loc.lon != LON_UNDEFINED) ||
           (strlen(conf.events[SUNRISE]) && strlen(conf.events[SUNSET])));
}

static void destroy(void) {
    /* Skeleton function needed for modules interface */
}

static void receive(const msg_t *const msg, const void* userdata) {
    if (!msg->is_pubsub) {
        read_timer(msg->fd_msg->fd);
        check_gamma();
    } else if (msg->ps_msg->type == USER) {
        MSG_TYPE();
        switch (type) {
            case LOCATION_UPD:
                location_callback();
                break;
            case CURRENT_BL_UPD:
                ambient_callback();
                break;
            case TEMP_REQ: {
                temp_upd *t = (temp_upd *)msg->ps_msg->message;
                interface_callback(t->new);
                }      
                break;
            default:
                break;
        }
    }
}

static void check_gamma(void) {
    const time_t t = time(NULL);
    const enum states old_state = state.time;
    const int old_in_event = state.in_event;
    const enum events old_target_event = target_event; 
    
    /*
     * get_gamma_events will always poll today events. It should not be necessary,
     * (as it will normally only be needed to get new events for tomorrow, once clight is started)
     * but if, for example, laptop gets suspended at evening, before computing next day events,
     * and it is waken next morning, it will proceed to compute "tomorrow" events, where tomorrow is
     * the wrong day (it should compute "today" events). Thus, avoid this kind of issues.
     */
    get_gamma_events(&t, state.current_loc.lat, state.current_loc.lon, 0);
    
    struct tm tm_now, tm_old;
    localtime_r(&t, &tm_now);
    localtime_r(&last_t, &tm_old);
    
    /* 
     * Properly reset long_transitioning when we change target event or we change current day.
     * This is needed when:
     * 1) we change target event (ie: when event_time_range == -conf.event_duration)
     * 2) we are suspended and:
     *      A) resumed with a different target_event
     *      B) resumed with same target_event but in a different day/year (well this is kinda overkill)
     */
    if (long_transitioning &&
        (old_target_event != target_event
        || tm_now.tm_yday != tm_old.tm_yday 
        || tm_now.tm_year != tm_old.tm_year)) {

        INFO("Long transition ended.\n");
        long_transitioning = 0;
    }
    
    /* If we switched time, emit signal */
    if (old_state != state.time) {
        time_msg.old = old_state;
        time_msg.new = state.time;
        M_PUB(&time_msg);
    }
    
    /* if we entered/left an event, emit signal */
    if (old_in_event != state.in_event) {
        in_ev_msg.old = old_in_event;
        in_ev_msg.new = state.in_event;
        M_PUB(&in_ev_msg);
    }

    /*
     * Forcefully set correct gamma every time
     * to avoid any possible sync issue
     * between time of day and gamma (eg after long suspend).
     * For long_transitioning, only call it when starting transition,
     * and at the end (to be sure to correctly set desired gamma and to avoid any sync issue)
     */
    if (!long_transitioning && !conf.ambient_gamma) {
        set_temp(conf.temp[state.time], &t);
    }
    
    /* desired gamma temp has been set. Set new GAMMA timer */
    time_t next = state.events[target_event] + event_time_range;
    INFO("Next alarm due to: %s", ctime(&next));
    set_timeout(next - t, 0, gamma_fd, 0);
    
    last_t = t;
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
    
    time_t old_events[2];
    old_events[SUNRISE] = state.events[SUNRISE];
    old_events[SUNSET] = state.events[SUNSET];

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
        
        evt_msg[SUNRISE].old = old_events[SUNRISE];
        evt_msg[SUNRISE].new = state.events[SUNRISE];
        M_PUB(&evt_msg[SUNRISE]);
        
        evt_msg[SUNSET].old = old_events[SUNSET];
        evt_msg[SUNSET].new = state.events[SUNSET];
        M_PUB(&evt_msg[SUNSET]);
    }
    check_next_event(now);
    check_state(now);
}

/*
 * Updates next_event global var, according to now time_t value.
 * Note that "+1" is because it seems timerfd receives timer end circa 1s in advance.
 */
static void check_next_event(const time_t *now) {
    /*
     * SUNRISE if:
     * We're after state.events[SUNSET] (when clight is started between SUNSET)
     * We're before state.events[SUNRISE] (when clight is started before today's SUNRISE)
     */
    if (*now + 1 > state.events[SUNSET] || *now + 1 < state.events[SUNRISE]) {
        state.time = NIGHT;
    } else {
        state.time = DAY;
    }
    target_event = (*now + 1 < (state.events[SUNRISE] + conf.event_duration)) ? SUNRISE : SUNSET;    
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
    if (labs(state.events[target_event] - (*now + 1)) <= conf.event_duration) {
        if (state.events[target_event] > *now + 1) {
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
}

static void set_temp(int temp, const time_t *now) {
    int ok, smooth, step, timeout;
    
    SYSBUS_ARG(args, CLIGHTD_SERVICE, "/org/clightd/clightd/Gamma", "org.clightd.clightd.Gamma", "Set");
    
    /* Compute long transition steps and timeouts (if outside of event, fallback to normal transition) */
    if (conf.gamma_long_transition && now && state.in_event) {
        smooth = 1;
        if (event_time_range == 0) {
            /* Remaining time in first half + second half of transition */
            timeout = (state.events[target_event] - *now) + conf.event_duration;
            temp = conf.temp[!state.time]; // use correct temp, ie the one for next event
        } else {
            /* Remaining time in second half of transition */
            timeout = conf.event_duration - (*now - state.events[target_event]);
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
        temp_msg.old = state.current_temp;
        state.current_temp = temp;
        temp_msg.new = state.current_temp;
        M_PUB(&temp_msg);
        if (!long_transitioning && conf.no_smooth_gamma) {
            INFO("%d gamma temp set.\n", temp);
        } else {
            INFO("%s transition to %d gamma temp started.\n", long_transitioning ? "Long" : "Normal", temp);
        }
    }
}

static void ambient_callback(void) {
    if (conf.ambient_gamma) {
        /* 
         * Note that conf.temp is not constant (it can be changed through bus api),
         * thus we have to always compute these ones.
         */
        const int diff = abs(conf.temp[DAY] - conf.temp[NIGHT]);
        const int min_temp = conf.temp[NIGHT] < conf.temp[DAY] ? conf.temp[NIGHT] : conf.temp[DAY]; 
        const int ambient_temp = (diff * state.current_bl_pct) + min_temp;
        set_temp(ambient_temp, NULL); // force refresh
    }
}

static void location_callback(void) {
    /* Updated GAMMA module sunrise/sunset for new location */
    state.events[SUNSET] = 0; // to force get_gamma_events to recheck sunrise and sunset for today
    set_timeout(0, 1, gamma_fd, 0);
    DEBUG("New position received. Updating sunrise and sunset times.\n");
}

static void interface_callback(int value) {
    conf.temp[state.time] = value;
    if (!conf.ambient_gamma) {
        set_temp(conf.temp[state.time], NULL); // force refresh (passing NULL time_t*)
    }
}
