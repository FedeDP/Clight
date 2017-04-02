#include "../inc/gamma.h"

#define ZENITH -0.83

static double  degToRad(const double angleDeg);
static double radToDeg(const double angleRad);
static float to_hours(const float rad);
static int calculate_sunrise_sunset(const float lat, const float lng, time_t *tt, const int type, int tomorrow);

enum types { SUNRISE, SUNSET };

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
 */
static int calculate_sunrise_sunset(const float lat, const float lng, time_t *tt, const int type, int tomorrow) {
    // 1. compute the day of the year (timeinfo->tm_yday below)
    time(tt);
    struct tm *timeinfo = gmtime(tt);
    if (!timeinfo) {
        return -1;
    }
    // if needed, set tomorrow
    timeinfo->tm_yday += tomorrow;
    timeinfo->tm_mday += tomorrow;

    // 2. convert the longitude to hour value and calculate an approximate time
    float lngHour = to_hours(lng);
    float t;
    if (type == SUNRISE) {
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
    if ((cosH > 1 && type == SUNRISE) || (cosH < -1 && type == SUNSET)) {
        return -2; // no sunrise/sunset today!
    }

    // 7b. finish calculating H and convert into hours
    float H;
    if (type == SUNRISE) {
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
    double seconds = modf(minutes, &minutes) * 60;

    // set correct values
    timeinfo->tm_hour = hours;
    timeinfo->tm_min = minutes;
    timeinfo->tm_sec = seconds;

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
 * Tries to understand next event:
 * first computes today's sunrise and sunset.
 * If now is < sunrise, next event is today's sunrise and we're during the night.
 * Otherwise, if now is < sunset, we're during the day and next event is sunset.
 * Else, next event will be tomorrow's sunrise and we're in night time.
 */
time_t get_next_gamma_alarm(const float lat, const float lon) {
    time_t sunrise, sunset, now;
    int ret_sunrise, ret_sunset;

    time(&now);

    ret_sunrise = calculate_sunrise(lat, lon, &sunrise, 0);
    ret_sunset = calculate_sunset(lat, lon, &sunset, 0);

    // if sunrise time is after now, set next alarm
    if (ret_sunrise == 0 && sunrise > now) {
        state.time = NIGHT;
        return sunrise;
    }
    if (ret_sunset == 0 && sunset > now) {
        state.time = DAY;
        return sunset;
    }
    // we are after sunset time for today. Let's check tomorrow's sunrise
    calculate_sunrise(lat, lon, &sunrise, 1);
    state.time = NIGHT;
    return sunrise;
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
    int new_temp;
    static int old_temp = 0;

    if (old_temp == 0) {
        struct bus_args args_get = {"org.clight.backlight", "/org/clight/backlight", "org.clight.backlight", "getgamma"};

        bus_call(&old_temp, "i", &args_get, "");
        if (state.quit) {
            return -1;
        }
    }

    if (old_temp != temp) {
        struct bus_args args_set = {"org.clight.backlight", "/org/clight/backlight", "org.clight.backlight", "setgamma"};

        if (conf.smooth_transition) {
            if (old_temp > temp) {
                    old_temp = old_temp - step < temp ? temp : old_temp - step;
                } else {
                    old_temp = old_temp + step > temp ? temp : old_temp + step;
                }
                bus_call(&new_temp, "i", &args_set, "i", old_temp);
        } else {
            // reset old_temp for next call
            old_temp = 0;
            bus_call(&new_temp, "i", &args_set, "i", temp);
        }
        printf("%d gamma temp setted.\n", new_temp);
    } else {
        // reset old_temp
        old_temp = 0;
        new_temp = temp;
        printf("Gamma temp was already %d\n", temp);
    }
    return new_temp != temp;
}
