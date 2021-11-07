#include "my_math.h"
#include "timer.h"
#include "interface.h"

static void receive_waiting_loc(const msg_t *const msg, UNUSED const void* userdata);
static void start_daytime(void);
static void check_daytime(void);
static void get_next_events(const time_t *now, const float lat, const float lon, int dayshift);
static void check_next_event(const time_t *now);
static void check_state(const time_t *now);
static void reset_daytime(void);
static int get_event(sd_bus *bus, const char *path, const char *interface, const char *property,
                     sd_bus_message *value, void *userdata, sd_bus_error *error);
static int set_event(sd_bus *bus, const char *path, const char *interface, const char *property,
              sd_bus_message *value, void *userdata, sd_bus_error *error);
static int set_os(sd_bus *bus, const char *path, const char *interface, const char *property,
              sd_bus_message *value, void *userdata, sd_bus_error *error);

static int day_fd;
static const sd_bus_vtable conf_daytime_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_WRITABLE_PROPERTY("Sunrise", "s", get_event, set_event, offsetof(daytime_conf_t, day_events[SUNRISE]), 0),
    SD_BUS_WRITABLE_PROPERTY("Sunset", "s", get_event, set_event, offsetof(daytime_conf_t, day_events[SUNSET]), 0),
    SD_BUS_WRITABLE_PROPERTY("Location", "(dd)", get_location, set_location, offsetof(daytime_conf_t, loc), 0),
    SD_BUS_WRITABLE_PROPERTY("EventDuration", "i", NULL, NULL, offsetof(daytime_conf_t, event_duration), 0),
    SD_BUS_WRITABLE_PROPERTY("SunriseOffset", "i", NULL, set_os, offsetof(daytime_conf_t, events_os[SUNRISE]), 0),
    SD_BUS_WRITABLE_PROPERTY("SunsetOffset", "i", NULL, set_os, offsetof(daytime_conf_t, events_os[SUNSET]), 0),
    SD_BUS_VTABLE_END
};

DECLARE_MSG(time_msg, DAYTIME_UPD);
DECLARE_MSG(in_ev_msg, IN_EVENT_UPD);
DECLARE_MSG(sunrise_msg, SUNRISE_UPD);
DECLARE_MSG(sunset_msg, SUNSET_UPD);
DECLARE_MSG(sunrise_req, SUNRISE_REQ);
DECLARE_MSG(sunset_req, SUNSET_REQ);
DECLARE_MSG(next_ev_msg, NEXT_DAYEVT_UPD);
DECLARE_MSG(temp_req, TEMP_REQ);

API(Daytime, conf_daytime_vtable, conf.day_conf);
MODULE("DAYTIME");

static void init(void) {
    temp_req.temp.daytime = -1;
    temp_req.temp.smooth = -1;
    
    M_SUB(LOC_UPD);
    M_SUB(SUNRISE_REQ);
    M_SUB(SUNSET_REQ);
    m_become(waiting_loc);
    
    init_Daytime_api();
}

static bool check(void) {
    return true;
}

static bool evaluate(void) {
    return !conf.wizard;
}

static void destroy(void) {
    deinit_Daytime_api();
}

static void receive_waiting_loc(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
    case LOC_UPD: {
        loc_upd *up = (loc_upd *)MSG_DATA();
        if (up->new.lat == LAT_UNDEFINED) {
            WARN("No location provider found; fallback to DAY daytime.\n");
            /* 
             * Stop ourself before sending message, 
             * this way DAYTIME_UPD message will be received from a stopped module
             * (to properly kill GAMMA)
             */
            m_stop();
            time_msg.day_time.new = DAY;
            M_PUB(&time_msg);
            state.day_time = DAY;
        } else {
            start_daytime();
        }
        break;
    }
    case SYSTEM_UPD: {
        /* 
         * If, when loop starts, LOCATION is not running, it means 
         * we need to startup immediately as we either have a fixed location
         * or fixed sunrise/sunset times.
         */
        if (msg->ps_msg->type == LOOP_STARTED) {
            const self_t *loc_ref = NULL;
            m_ref("LOCATION", &loc_ref);
            if (!module_is(loc_ref, RUNNING)) {
                start_daytime();
            }
        }
        break;
    }
    default:
        break;
    }
}

static void receive(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
        case FD_UPD:
            read_timer(msg->fd_msg->fd);
            check_daytime();
            break;
        case LOC_UPD:
            reset_daytime();
            DEBUG("New position received. Updating sunrise and sunset times.\n");
            break;
        case SUNSET_REQ:
        case SUNRISE_REQ: {
            evt_upd *up = (evt_upd *)MSG_DATA();
            if (VALIDATE_REQ(up)) {
                if (MSG_TYPE() == SUNRISE_REQ) {
                    strncpy(conf.day_conf.day_events[SUNRISE], up->event, sizeof(conf.day_conf.day_events[SUNRISE]));
                } else {
                    strncpy(conf.day_conf.day_events[SUNSET], up->event, sizeof(conf.day_conf.day_events[SUNSET]));
                }
                reset_daytime();
            }
            break;
        }
        default:
            break;
    }
}

static void start_daytime(void) {
    day_fd = start_timer(CLOCK_BOOTTIME, 0, 1);
    m_register_fd(day_fd, true, NULL);
    m_unbecome();
}

static void check_daytime(void) {
    const time_t t = time(NULL);
    const enum day_states old_daytime = state.day_time;
    const int old_in_event = state.in_event;
    const enum day_events old_next_event = state.next_event; 
    
    /*
     * get_gamma_events will always poll today events. It should not be necessary,
     * (as it will normally only be needed to get new events for tomorrow, once clight is started)
     * but if, for example, laptop gets suspended at evening, before computing next day events,
     * and it is waken next morning, it will proceed to compute "tomorrow" events, where tomorrow is
     * the wrong day (it should compute "today" events). Thus, avoid this kind of issues.
     */
    get_next_events(&t, state.current_loc.lat, state.current_loc.lon, 0);
        
    /** Check which messages should be published **/
    
    /* 
     * If we changed old_daytime, emit signal.
     * 
     * THIS MUST BE SENT AS FIRST as gamma waits this message
     * to actually start and become running.
     */
    if (old_daytime != state.day_time) {
        time_msg.day_time.old = old_daytime;
        time_msg.day_time.new = state.day_time;
        M_PUB(&time_msg);
    }
    
    /* If we changed next event, emit signal */
    if (old_next_event != state.next_event) {
        next_ev_msg.nextevt.old = old_next_event;
        next_ev_msg.nextevt.new = state.next_event;
        M_PUB(&next_ev_msg);
    }
        
    /* if we entered/left an event, emit signal */
    if (old_in_event != state.in_event) {
        in_ev_msg.day_time.old = old_in_event;
        in_ev_msg.day_time.new = state.in_event;
        M_PUB(&in_ev_msg);
    }
        
    /**                                 **/
    
    /*
     * Forcefully set correct gamma every time
     * to avoid any possible sync issue
     * between time of day and gamma (eg after long suspend).
     */
    if (!conf.gamma_conf.disabled) {
        temp_req.temp.new = conf.gamma_conf.temp[state.day_time];
        M_PUB(&temp_req);
    }

    const time_t next = state.day_events[state.next_event] + conf.day_conf.events_os[state.next_event] + state.event_time_range;
    INFO("Next alarm due to: %s", ctime(&next));
    set_timeout(next - t, 0, day_fd, 0);
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
static void get_next_events(const time_t *now, const float lat, const float lon, int dayshift) {
    time_t t;
    const time_t old_events[SIZE_EVENTS] = { state.day_events[SUNRISE], state.day_events[SUNSET] };
    
    /* only every new day, after today's last event (ie: sunset + event_duration) */
    if (*now + 1 >= state.day_events[SUNSET] + conf.day_conf.event_duration) {
        if (calculate_sunset(lat, lon, &t, dayshift) == 0) {
            /* If today's sunset was before now, compute tomorrow */
            if (*now + 1 >= t + conf.day_conf.event_duration) {
                /*
                 * we're between today's sunset and tomorrow sunrise.
                 * rerun function with tomorrow.
                 */
                return get_next_events(now, lat, lon, ++dayshift);
            }
            state.day_events[SUNSET] = t;
        } else {
            state.day_events[SUNSET] = -1;
        }
        
        if (calculate_sunrise(lat, lon, &t, dayshift) == 0) {
            /*
             * Force computation of today event if SUNRISE is
             * not today; eg: in local time it is at 6am, but utc time is 22,
             * so it counts as today while it is indeed tomorrow...
             */
            if (t > state.day_events[SUNSET]) {
                calculate_sunrise(lat, lon, &t, dayshift - 1);
            }
            
            state.day_events[SUNRISE] = t;
        } else {
            state.day_events[SUNRISE] = -1;
        }
        
        if (state.day_events[SUNRISE] == -1 && state.day_events[SUNSET] == -1) {
            /*
             * no sunrise/sunset could be found.
             * Assume day and set sunset 12hrs from now
             */
            state.day_events[SUNSET] = *now + 12 * 60 * 60;
            WARN("Failed to retrieve sunrise/sunset informations.\n");
        }
        
        sunrise_msg.event.old = old_events[SUNRISE];
        sunrise_msg.event.new = state.day_events[SUNRISE];
        M_PUB(&sunrise_msg);
        
        sunset_msg.event.old = old_events[SUNSET];
        sunset_msg.event.new = state.day_events[SUNSET];
        M_PUB(&sunset_msg);
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
    if (*now + 1 > state.day_events[SUNSET] || *now + 1 < state.day_events[SUNRISE]) {
        state.day_time = NIGHT;
    } else {
        state.day_time = DAY;
    }
    state.next_event = (*now + 1 < (state.day_events[SUNRISE] + conf.day_conf.event_duration)) ? SUNRISE : SUNSET;    
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
    if (labs(state.day_events[state.next_event] - (*now + 1)) <= conf.day_conf.event_duration) {
        if (state.day_events[state.next_event] > *now + 1) {
            state.event_time_range = 0; // next timer is on next_event
        } else {
            state.event_time_range = conf.day_conf.event_duration; // next timer is when leaving event
        }
        state.in_event = true;
        DEBUG("Currently inside an event.\n");
    } else {
        state.event_time_range = -conf.day_conf.event_duration; // next timer is entering next event
        state.in_event = false;
    }
}

static void reset_daytime(void) {
    /* Updated sunrise/sunset times for new location */
    state.day_events[SUNSET] = 0; // to force get_next_events to recheck sunrise and sunset for today
    set_timeout(0, 1, day_fd, 0);
}

static int get_event(sd_bus *bus, const char *path, const char *interface, const char *property,
              sd_bus_message *value, void *userdata, sd_bus_error *error) {
    return sd_bus_message_append(value, "s", userdata);
}

static int set_event(sd_bus *bus, const char *path, const char *interface, const char *property,
                            sd_bus_message *value, void *userdata, sd_bus_error *error) {
    const char *event = NULL;
    VALIDATE_PARAMS(value, "s", &event);
    
    message_t *msg = &sunrise_req;
    if (userdata == &conf.day_conf.day_events[SUNSET]) {
        msg = &sunset_req;
    }
    strncpy(msg->event.event, event, sizeof(msg->event.event));
    M_PUB(msg);
    return r;
}

static int set_os(sd_bus *bus, const char *path, const char *interface, const char *property,
                  sd_bus_message *value, void *userdata, sd_bus_error *error) {
    VALIDATE_PARAMS(value, "i", userdata);
    reset_daytime();
    return r;
}
