#include "../inc/gamma.h"

#define ZENITH -0.83

static double  degToRad(const double angleDeg);
static double radToDeg(const double angleRad);
static float to_hours(const float rad);
static int calculate_sunrise_sunset(const float lat, const float lng, time_t *tt, const int type, int tomorrow);
static int set_temp(int temp);

enum types { SUNRISE, SUNSET };
enum states { DAY, NIGHT };

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

void check_gamma_time(const float lat, const float lon, struct time *t) {
    time_t sunrise, sunset, now;
    int ret_sunrise, ret_sunset;

    time(&now);

    ret_sunrise = calculate_sunrise(lat, lon, &sunrise, 0);
    ret_sunset = calculate_sunset(lat, lon, &sunset, 0);

    // if sunrise time is after now, set next alarm
    if (ret_sunrise == 0 && sunrise > now) {
        t->state = NIGHT;
        t->next_alarm = sunrise;
    } else if (ret_sunset == 0 && sunset > now) {
        t->state = DAY;
        t->next_alarm = sunset;
    } else {
        // we are after sunset time for today. Let's check tomorrow's sunrise
        calculate_sunrise(lat, lon, &sunrise, 1);
        t->state = NIGHT;
        t->next_alarm = sunrise;
    }
}

static int set_temp(int temp) {
    const int step = 50;
    int new_temp;
    static int old_temp = 0;

    if (old_temp == 0) {
        bus_call(&old_temp, "i", "getgamma", "");
    }

    if (old_temp != temp) {
        if (conf.smooth_transition) {
            if (old_temp > temp) {
                    old_temp = old_temp - step < temp ? temp : old_temp - step;
                } else {
                    old_temp = old_temp + step > temp ? temp : old_temp + step;
                }
                bus_call(&new_temp, "i", "setgamma", "i", old_temp);
        } else {
            // reset old_temp
            old_temp = 0;
            bus_call(&new_temp, "i", "setgamma", "i", temp);
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


int set_screen_temp(int status) {
    switch (status) {
        case DAY:
            return set_temp(conf.day_temp);
        case NIGHT:
            return set_temp(conf.night_temp);
        default:
            return -1;
    }
}
