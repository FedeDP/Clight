#include "../inc/gamma.h"
#include "../inc/gamma_smooth.h"
#include "../inc/location.h"

#define ZENITH -0.83

static void init(void);
static int check(void);
static void destroy(void);
static void callback(void);
static void check_gamma(void);
static double  degToRad(const double angleDeg);
static double radToDeg(const double angleRad);
static float to_hours(const float rad);
static int calculate_sunrise_sunset(const float lat, const float lng,
                                    time_t *tt, enum events event, int tomorrow);
static int calculate_sunrise(const float lat, const float lng, time_t *tt, int tomorrow);
static int calculate_sunset(const float lat, const float lng, time_t *tt, int tomorrow);
static void get_gamma_events(time_t *now, const float lat, const float lon, int day);
static void check_next_event(time_t *now);
static void check_state(time_t *now);
static void location_callback(void);

static struct dependency dependencies[] = { {HARD, BUS}, {HARD, LOCATION} };
static struct self_t self = {
    .name = "Gamma",
    .idx = GAMMA,
    .num_deps = SIZE(dependencies),
    .deps =  dependencies
};

void set_gamma_self(void) {
    SET_SELF();
}

static void init(void) {
    int gamma_timerfd = start_timer(CLOCK_REALTIME, 0, 1);
    init_module(gamma_timerfd, self.idx);
    if (!modules[self.idx].disabled) {
        struct bus_cb loc_cb = { LOCATION, location_callback };
        add_mod_callback(loc_cb);
    }
}

static int check(void) {
    return  conf.no_gamma || 
            !state.display || 
            !state.xauthority;
}

static void destroy(void) {
    /* Skeleton function needed for modules interface */
}

static void callback(void) {
    uint64_t t;

    read(main_p[self.idx].fd, &t, sizeof(uint64_t));    
    check_gamma();
}

/*
 * checks next day events (or current day if clight has started right now).
 * Then, if needed (ie: first time this func is called or when state.time changed),
 * calls set_temp with correct temp, given current state(night or day).
 * It returns 0 if: smooth_transition is off or desired temp has finally been setted (after transitioning).
 * If it returns 0, reset transitioning flag and set next event timeout.
 * Else, set a timeout for smooth transition and set transitioning flag to 1.
 * If ret == 0, it can also mean we haven't called set_temp, and this means an
 * "event" timeout elapsed. If old_state != state.time (ie: if we entered or left EVENT state),
 * set new BRIGHTNESS correct timeout according to new state.
 */
static void check_gamma(void) {
    time_t t = time(NULL);
    /*
     * get_gamma_events will always poll today events. It should not be necessary,
     * (as it will normally only be needed to get new events for tomorrow, once clight is started)
     * but if, for example, laptop gets suspended at evening, before computing next day events,
     * and it is waken next morning, it will proceed to compute "tomorrow" events, where tomorrow is
     * the wrong day (it should compute "today" events). Thus, avoid this kind of issue.
     */
    enum states old_state = state.time;
    get_gamma_events(&t, conf.lat, conf.lon, 0);

    if (state.event_time_range == conf.event_duration) {
        if (conf.no_gamma_smooth_transition) {
            set_temp(conf.temp[state.time]);
        } else if (modules[GAMMA_SMOOTH].inited) {
            start_gamma_transition(1);
        }
    }

    /* desired gamma temp has been setted. Set new GAMMA timer and reset transitioning state. */
    t = state.events[state.next_event] + state.event_time_range;
    INFO("Next gamma alarm due to: %s", ctime(&t));
    set_timeout(t, 0, main_p[self.idx].fd, TFD_TIMER_ABSTIME);

    /* if we entered/left an event, set correct timeout to BRIGHTNESS */
    if (old_state != state.time && modules[BRIGHTNESS].inited && !state.fast_recapture) {
        reset_timer(main_p[BRIGHTNESS].fd, conf.timeout[state.ac_state][old_state], conf.timeout[state.ac_state][state.time]);
    }
}

/* 
 * Convert degrees to radians 
 */
static double  degToRad(double angleDeg) {
    return (M_PI * angleDeg / 180.0);
}

/* 
 * Convert radians to degrees 
 */
static double radToDeg(double angleRad) {
    return (180.0 * angleRad / M_PI);
}

static float to_hours(const float rad) {
    return rad / 15.0;// 360 degree / 24 hours = 15 degrees/h
}

/*
 * Just a small function to compute sunset/sunrise for today (or tomorrow).
 * See: http://stackoverflow.com/questions/7064531/sunrise-sunset-times-in-c
 * IF conf.events are both set, it means sunrise/sunset times are user-setted.
 * So, only store in *tt their corresponding time_t values.
 */
static int calculate_sunrise_sunset(const float lat, const float lng, time_t *tt, enum events event, int tomorrow) {
    // 1. compute the day of the year (timeinfo->tm_yday below)
    time(tt);
    struct tm *timeinfo;

    if (strlen(conf.events[SUNRISE]) > 0 && strlen(conf.events[SUNSET]) > 0) {
        timeinfo = localtime(tt);
    } else {
        timeinfo = gmtime(tt);
    }
    if (!timeinfo) {
        return -1;
    }
    // if needed, set tomorrow
    timeinfo->tm_yday += tomorrow;
    timeinfo->tm_mday += tomorrow;

    /* If user provided a sunrise/sunset time, use them */
    if (strlen(conf.events[SUNRISE]) > 0 && strlen(conf.events[SUNSET]) > 0) {
        char *s = strptime(conf.events[event], "%R", timeinfo);
        if (!s) {
            ERROR("Wrong sunrise/sunset time setted by a cmdline arg. Leaving.\n");
            return -1;
        }
        timeinfo->tm_sec = 0;
        *tt = mktime(timeinfo);
        return 0;
    }

    // 2. convert the longitude to hour value and calculate an approximate time
    float lngHour = to_hours(lng);
    float t;
    if (event == SUNRISE) {
        t = timeinfo->tm_yday + (6 - lngHour) / 24;
    } else {
        t = timeinfo->tm_yday + (18 - lngHour) / 24;
    }

    // 3. calculate the Sun's mean anomaly
    float M = (0.9856 * t) - 3.289;

    // 4. calculate the Sun's true longitude
    float L = fmod(M + 1.916 * sin(degToRad(M)) + 0.020 * sin(2 * degToRad(M)) + 282.634, 360.0);

    // 5a. calculate the Sun's right ascension
    float RA = fmod(radToDeg(atan(0.91764 * tan(degToRad(L)))), 360.0);

    // 5b. right ascension value needs to be in the same quadrant as L
    float Lquadrant  = floor(L/90) * 90;
    float RAquadrant = floor(RA/90) * 90;
    RA += (Lquadrant - RAquadrant);

    // 5c. right ascension value needs to be converted into hours
    RA = to_hours(RA);

    // 6. calculate the Sun's declination
    float sinDec = 0.39782 * sin(degToRad(L));
    float cosDec = cos(asin(sinDec));

    // 7a. calculate the Sun's local hour angle
    float cosH = sin(degToRad(ZENITH)) - (sinDec * sin(degToRad(lat))) / (cosDec * cos(degToRad(lat)));
    if ((cosH > 1 && event == SUNRISE) || (cosH < -1 && event == SUNSET)) {
        return -2; // no sunrise/sunset today!
    }

    // 7b. finish calculating H and convert into hours
    float H;
    if (event == SUNRISE) {
        H = 360 - radToDeg(acos(cosH));
    } else {
        H = radToDeg(acos(cosH));
    }
    H = to_hours(H);

    // 8. calculate local mean time of rising/setting
    float T = H + RA - (0.06571 * t) - 6.622;

    // 9. adjust back to UTC
    float UT = fmod(24 + fmod(T - lngHour,24.0), 24.0);

    double hours;
    double minutes = modf(UT, &hours) * 60;

    // set correct values
    timeinfo->tm_hour = hours;
    timeinfo->tm_min = minutes;
    timeinfo->tm_sec = 0;

    // store in user provided ptr correct data
    *tt = timegm(timeinfo);
    if (*tt == (time_t) -1) {
        return -1;
    }
    return 0;
}

static int calculate_sunrise(const float lat, const float lng, time_t *tt, int tomorrow) {
    return calculate_sunrise_sunset(lat, lng, tt, SUNRISE, tomorrow);
}

static int calculate_sunset(const float lat, const float lng, time_t *tt, int tomorrow) {
    return calculate_sunrise_sunset(lat, lng, tt, SUNSET, tomorrow);
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
static void get_gamma_events(time_t *now, const float lat, const float lon, int day) {
    time_t t;

    /* only every new day, after latest event of today finished */
    if (*now + 1 >= state.events[SUNSET] + conf.event_duration) {
        if (calculate_sunset(lat, lon, &t, day) == 0) {
            if (*now + 1 >= t + conf.event_duration) {
                /*
                 * we're between today's sunrise and tomorrow sunrise.
                 * rerun function with tomorrow.
                 * Useful only first time clight is started
                 * (ie: if it is started at night time)
                 */
                return get_gamma_events(now, lat, lon, ++day);
            }
            state.events[SUNSET] = t;
        } else {
            state.events[SUNSET] = -1;
        }

        if (calculate_sunrise(lat, lon, &t, day) == 0) {
            state.events[SUNRISE] = t;
        } else {
            state.events[SUNRISE] = -1;
        }

        if (state.events[SUNRISE] == -1 && state.events[SUNSET] == -1) {
            // no sunrise/sunset could be found.
            state.time = UNKNOWN;
            // set an alarm to recheck gamma event after 12h...
            state.events[SUNRISE] = *now + 12 * 60 * 60;
            state.next_event = SUNRISE;
            WARN("Failed to retrieve sunrise/sunset informations.\n");
            return;
        }
    }
    check_next_event(now);
    check_state(now);
}

/*
 * Updates state.next_event global var, according to now time_t value.
 * Note that "+1" is because it seems timerfd receives timer end circa 1s in advance.
 */
static void check_next_event(time_t *now) {
    if (*now + 1 < state.events[SUNRISE] + conf.event_duration || state.events[SUNSET] == -1) {
        state.next_event = SUNRISE;
    } else {
        state.next_event = SUNSET;
    }
}

/*
 * Updates state.time global var, according to now time_t value.
 * Note that "+1" is because it seems timerfd receives timer end circa 1s in advance.
 * If we're inside an event, checks which side of the events we're in
 * (to understand which conf.temp is correct for this state).
 * Then sets state.event_time_range accordingly; ie: 30mins before event, if we're not inside an event;
 * 0 if we just entered an event (so next_event has to be exactly event time, to set new temp),
 * 30mins after event to remove EVENT state.
 */
static void check_state(time_t *now) {
    if (labs(state.events[state.next_event] - (*now + 1)) <= conf.event_duration) {
        int event_t;

        if (state.events[state.next_event] > *now + 1) {
            event_t = state.next_event;
            state.event_time_range = 0;
        } else {
            event_t = !state.next_event;
            state.event_time_range = conf.event_duration;
        }
        conf.temp[EVENT] = event_t == SUNRISE ? conf.temp[NIGHT] : conf.temp[DAY];
        state.time = EVENT;
        DEBUG("Currently inside an event.\n");
    } else {
        state.time = state.next_event == SUNRISE ? NIGHT : DAY;
        state.event_time_range = -conf.event_duration; // 30mins before event
    }
}

static void location_callback(void) {
    /* Updated GAMMA module sunrise/sunset for new location */
    state.events[SUNSET] = 0; // to force get_gamma_events to recheck sunrise and sunset for today
    set_timeout(0, 1, main_p[self.idx].fd, 0);
}
