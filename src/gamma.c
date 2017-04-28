#include "../inc/gamma.h"

#define ZENITH -0.83
#define SMOOTH_TRANSITION_TIMEOUT 300 * 1000 * 1000

static void init(void);
static void destroy(void);
static void gamma_cb(void);
static void check_gamma(void);
static double  degToRad(const double angleDeg);
static double radToDeg(const double angleRad);
static float to_hours(const float rad);
static int calculate_sunrise_sunset(const float lat, const float lng,
                                    time_t *tt, enum events event, int tomorrow);
static void get_gamma_events(time_t *now, const float lat, const float lon, int day);
static void check_next_event(time_t *now);
static void check_state(time_t *now);
static int set_temp(int temp);

static struct dependency dependencies[] = { {HARD, BUS_IX}, {HARD, LOCATION_IX} };
static struct self_t self = {
    .name = "Gamma",
    .idx = GAMMA_IX,
    .num_deps = SIZE(dependencies),
    .deps =  dependencies
};

void set_gamma_self(void) {
    modules[self.idx].self = &self;
    modules[self.idx].init = init;
    modules[self.idx].destroy = destroy;
    set_self_deps(&self);
}

static void init(void) {
    int gamma_timerfd = start_timer(CLOCK_REALTIME, 1, 0);
    init_module(gamma_timerfd, self.idx, gamma_cb);
}

static void destroy(void) {
    if (main_p[self.idx].fd > 0) {
        close(main_p[self.idx].fd);
    }
}

static void gamma_cb(void) {
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
 * set new CAPTURE_IX correct timeout according to new state.
 */
static void check_gamma(void) {
    static int transitioning = 0, first_time = 1;
    time_t t;
    enum states old_state = state.time;

    /*
     * Only if we're not doing a smooth transition
     */
    if (!transitioning) {
        t = time(NULL);
        /*
         * first time clight is started, get_gamma_events will poll today events.
         * Then, it will be called every day after end of last event (ie: sunset + 30mins)
         */
        get_gamma_events(&t, conf.lat, conf.lon, state.events[SUNSET] != 0);
        /* calculate_sunrise_sunset will fail if wrong sunrise/sunset strings are passed from options */
        if (state.quit) {
            return;
        }
    }

    int ret = 0;
    if (transitioning || state.event_time_range == conf.event_duration || first_time) {
        first_time = 0;
        ret = set_temp(conf.temp[state.time]); // ret = -1 if an error happens
    }

    /* desired gamma temp has been setted. Set new GAMMA_IX timer and reset transitioning state. */
    if (ret == 0) {
        t = state.events[state.next_event] + state.event_time_range;
        INFO("Next gamma alarm due to: %s", ctime(&t));
        set_timeout(t, 0, main_p[self.idx].fd, TFD_TIMER_ABSTIME);
        transitioning = 0;

        /* if we entered/left an event, set correct timeout to CAPTURE_IX */
        if (old_state != state.time && modules[CAPTURE_IX].inited) {
            unsigned int elapsed_time = conf.timeout[old_state] - get_timeout(main_p[CAPTURE_IX].fd);
            /* if we still need to wait some seconds */
            if (conf.timeout[state.time] - elapsed_time > 0) {
                set_timeout(conf.timeout[state.time] - elapsed_time, 0, main_p[CAPTURE_IX].fd, 0);
            } else {
                /* with new timeout, old_timeout would already been elapsed */
                set_timeout(0, 1, main_p[CAPTURE_IX].fd, 0);
            }
        }
    } else if (ret == 1) {
        /* We are still in a gamma transition. Set a timeout of 300ms for smooth transition */
        set_timeout(0, SMOOTH_TRANSITION_TIMEOUT, main_p[self.idx].fd, 0);
        transitioning = 1;
    }
}

/* Convert degrees to radians */
static double  degToRad(double angleDeg) {
    return (M_PI * angleDeg / 180.0);
}

/* Convert radians to degrees */
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
    } else {
        state.time = state.next_event == SUNRISE ? NIGHT : DAY;
        state.event_time_range = -conf.event_duration; // 30mins before event
    }
}

/*
 * First, it gets current gamma value.
 * Then, if current value is != from temp, it will adjust screen temperature accordingly.
 * If smooth_transition is enabled, the function will return 1 until they are the same.
 * old_temp is static so we don't have to call getgamma everytime the function is called (if smooth_transition is enabled.)
 * and gets resetted when old_temp reaches correct temp.
 */
static int set_temp(int temp) {
    const int step = 50;
    int new_temp;
    static int old_temp = 0;

    if (temp == -1) {
        return 0;
    }

    if (old_temp == 0) {
        struct bus_args args_get = {"org.clightd.backlight", "/org/clightd/backlight", "org.clightd.backlight", "getgamma"};

        bus_call(&old_temp, "i", &args_get, "ss", getenv("DISPLAY"), getenv("XAUTHORITY"));
        if (state.quit) {
            return -1;
        }
    }

    if (old_temp != temp) {
        struct bus_args args_set = {"org.clightd.backlight", "/org/clightd/backlight", "org.clightd.backlight", "setgamma"};

        if (!conf.no_smooth_transition) {
            if (old_temp > temp) {
                    old_temp = old_temp - step < temp ? temp : old_temp - step;
                } else {
                    old_temp = old_temp + step > temp ? temp : old_temp + step;
                }
                bus_call(&new_temp, "i", &args_set, "ssi", getenv("DISPLAY"), getenv("XAUTHORITY"), old_temp);
        } else {
            bus_call(&new_temp, "i", &args_set, "ssi", getenv("DISPLAY"), getenv("XAUTHORITY"), temp);
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
