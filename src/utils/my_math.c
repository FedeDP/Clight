#include <gsl/gsl_multifit.h>
#include <gsl/gsl_statistics_double.h>
#include "my_math.h"
#include "utils.h"

#define ZENITH -0.83

static void plot_poly_curve(curve_t *curve);
static char **init_grid(int num_points);
static void show_grid(char **grid, int num_points);
static void free_grid(char **grid, int num_points);
static float to_hours(const float rad);
static int calculate_sunrise_sunset(const float lat, const float lng, time_t *tt, enum day_events event, int dayshift);

/*
 * Convert degrees to radians
 */
double degToRad(double angleDeg) {
    return (M_PI * angleDeg / 180.0);
}

/*
 * Convert radians to degrees
 */
double radToDeg(double angleRad) {
    return (180.0 * angleRad / M_PI);
}

/*
 * Compute mean and normalize between 0-1
 */
double compute_average(const double *intensity, int num) {
    double mean = gsl_stats_mean(intensity, 1, num);
    return mean;
}

/*
 * Big thanks to https://rosettacode.org/wiki/Polynomial_regression#C
 */
void polynomialfit(double *XPoints, curve_t *curve, const char *tag) {
    double chisq;
    
    gsl_matrix *X = gsl_matrix_alloc(curve->num_points, DEGREE);
    gsl_vector *y = gsl_vector_alloc(curve->num_points);
    gsl_vector *c = gsl_vector_alloc(DEGREE);
    gsl_matrix *cov = gsl_matrix_alloc(DEGREE, DEGREE);
    
    for (int i = 0; i < curve->num_points; i++) {
        for (int j = 0; j < DEGREE; j++) {
            if (XPoints) {
                gsl_matrix_set(X, i, j, pow(XPoints[i], j));
            } else {
                gsl_matrix_set(X, i, j, pow(i, j));
            }
        }
        gsl_vector_set(y, i, curve->points[i]);
    }
    
    gsl_multifit_linear_workspace *ws = gsl_multifit_linear_alloc(curve->num_points, DEGREE);
    gsl_multifit_linear(X, y, c, cov, &chisq, ws);
    
    /* store results */
    for(int i = 0; i < DEGREE; i++) {
        curve->fit_parameters[i] = gsl_vector_get(c, i);
    }
    
    gsl_multifit_linear_free(ws);
    gsl_matrix_free(X);
    gsl_matrix_free(cov);
    gsl_vector_free(y);
    gsl_vector_free(c);
    
    DEBUG("%s curve: y = %lf + %lfx + %lfx^2\n", tag, curve->fit_parameters[0], curve->fit_parameters[1], curve->fit_parameters[2]);
    plot_poly_curve(curve);
}

double clamp(double value, double max, double min) {
    if (value > max) {
        return max;
    }
    if (value < min) {
        return min;
    }
    return value;
}

double get_value_from_curve(const double perc, curve_t *curve) {
    // Keyboard backlight curves are upside down
    const double max = curve->points[curve->num_points - 1] > curve->points[0] ? curve->points[curve->num_points - 1] : curve->points[0];
    const double min = curve->points[0] < curve->points[curve->num_points - 1] ? curve->points[0] : curve->points[curve->num_points - 1];
    
    /* y = a0 + a1x + a2x^2 */
    const double real_perc = perc * (curve->num_points - 1);
    const double b = curve->fit_parameters[0] 
                    + curve->fit_parameters[1] * real_perc 
                    + curve->fit_parameters[2] * pow(real_perc, 2);
    const double value = clamp(b, max, min);
    return value;
}

static void plot_poly_curve(curve_t *curve) {
    const int maxY = curve->num_points - 1;
    char **grid = init_grid(curve->num_points);
    for (int i = 0; i < curve->num_points; i++) {
        const double val = get_value_from_curve((double)i / (curve->num_points - 1), curve);
        const int y = round(val * (curve->num_points - 1));
        grid[maxY - y][i] = '*';
    }
    show_grid(grid, curve->num_points);
    free_grid(grid, curve->num_points);
}

static char **init_grid(int num_points) {
    char **grid = calloc(num_points, sizeof(char *));
    for (int i = 0; i < num_points; i++) {
        grid[i] = calloc(num_points, sizeof(char));
    }
    
    for (int y = 0; y < num_points; y++) {
        for (int x = 0; x < num_points; x++) {
            grid[y][x] = ' ';
        }
    }
    /* draw the axis */
    for(int y = 0; y < num_points; y++) {
        grid[y][0] = '|';
    }
    for(int x = 0; x < num_points; x++) {
        grid[num_points - 1][x] = '-';
    }
    grid[num_points - 1][0] = '+';
    return grid;
}

static void show_grid(char **grid, int num_points) {
    PLOT("BL\n^\n");
    for(int y = 0; y < num_points; y++) {
        for(int x = 0; x < num_points; x++) {
            PLOT("%c", grid[y][x]);
        }
        
        if (y != num_points - 1) {
            PLOT("\n");
        }
    }
    PLOT(">BR\n");
}

static void free_grid(char **grid, int num_points) {
     for (int i = 0; i < num_points; i++) {
        free(grid[i]);
    }
    free(grid);
}

static float to_hours(const float rad) {
    return rad / 15.0;// 360 degree / 24 hours = 15 degrees/h
}

/*
 * Just a small function to compute sunset/sunrise for today (or tomorrow).
 * See: http://stackoverflow.com/questions/7064531/sunrise-sunset-times-in-c
 * If conf.events[event] is set, it means "event" time is user-set.
 * So, only store in *tt its corresponding time_t values.
 */
static int calculate_sunrise_sunset(const float lat, const float lng, time_t *tt, enum day_events event, int dayshift) {
    // 1. compute the day of the year (timeinfo->tm_yday below)
    time(tt);
    struct tm *timeinfo = localtime(tt);
    if (!timeinfo) {
        return -1;
    }
    // if needed, set dayshift
    timeinfo->tm_yday += dayshift;
    timeinfo->tm_mday += dayshift;
    timeinfo->tm_sec = 0;

    /* If user provided a sunrise/sunset time, use them */
    if (!is_string_empty(conf.day_conf.day_events[event])) {
        strptime(conf.day_conf.day_events[event], "%R", timeinfo);
        *tt = mktime(timeinfo);
        return 0;
    }

    // 2. convert the longitude to hour value and calculate an approximate time
    float lngHour = to_hours(lng);
    float t;
    if (event == SUNRISE) {
        t = timeinfo->tm_yday + (6.0 - lngHour) / 24.0;
    } else {
        t = timeinfo->tm_yday + (18.0 - lngHour) / 24.0;
    }

    // 3. calculate the Sun's mean anomaly
    float M = (0.9856 * t) - 3.289;

    // 4. calculate the Sun's true longitude
    float L = fmod(M + 1.916 * sin(degToRad(M)) + 0.020 * sin(2 * degToRad(M)) + 282.634, 360.0);

    // 5a. calculate the Sun's right ascension
    float RA = fmod(radToDeg(atan(0.91764 * tan(degToRad(L)))), 360.0);

    // 5b. right ascension value needs to be in the same quadrant as L
    float Lquadrant = floor(L / 90) * 90;
    float RAquadrant = floor(RA / 90) * 90;
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
        H = 360.0 - radToDeg(acos(cosH));
    } else {
        H = radToDeg(acos(cosH));
    }
    H = to_hours(H);

    // 8. calculate local mean time of rising/setting
    float T = H + RA - (0.06571 * t) - 6.622;

    // 9. adjust back to UTC
    float UT = fmod(24 + fmod(T - lngHour, 24.0), 24.0);

    double hours;
    double minutes = modf(UT, &hours) * 60;

    // set correct values
    timeinfo->tm_hour = hours;
    timeinfo->tm_min = minutes;

    // store in user provided ptr correct data
    *tt = timegm(timeinfo);
    if (*tt == (time_t) -1) {
        return -1;
    }
    return 0;
}

int calculate_sunrise(const float lat, const float lng, time_t *tt, int dayshift) {
    return calculate_sunrise_sunset(lat, lng, tt, SUNRISE, dayshift);
}

int calculate_sunset(const float lat, const float lng, time_t *tt, int dayshift) {
    return calculate_sunrise_sunset(lat, lng, tt, SUNSET, dayshift);
}

/*
 * Get distance between 2 locations
 */
double get_distance(loc_t *loc1, loc_t *loc2) {
    double theta = loc1->lon - loc2->lon;
    double dist = sin(degToRad(loc1->lat)) * sin(degToRad(loc2->lat)) + cos(degToRad(loc1->lat)) * cos(degToRad(loc2->lat)) * cos(degToRad(theta));
    dist = acos(dist);
    dist = radToDeg(dist);
    dist *= 60 * 1.1515;
    DEBUG("Loc distance: %.2lf,%.2lf -> %.2lf,%.2lf : %.2lf km.\n", loc1->lat, loc1->lon, loc2->lat, loc2->lon, dist);
    return dist;
}
