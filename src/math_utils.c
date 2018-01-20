#include "../inc/math_utils.h"
#include <gsl/gsl_multifit.h>
#include <gsl/gsl_statistics_double.h>

/*
 * Get distance between 2 locations
 */
double get_distance(struct location loc1, struct location loc2) {
    double theta, dist;
    theta = loc1.lon - loc2.lon;
    dist = sin(degToRad(loc1.lat)) * sin(degToRad(loc2.lat)) + cos(degToRad(loc1.lat)) * cos(degToRad(loc2.lat)) * cos(degToRad(theta));
    dist = acos(dist);
    dist = radToDeg(dist);
    dist = dist * 60 * 1.1515;
    return (dist);
}

/*
 * Convert degrees to radians
 */
double  degToRad(double angleDeg) {
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
double compute_average(double *intensity, int num) {
    double mean = gsl_stats_mean(intensity, 1, num) / 255;
    DEBUG("Average frames brightness: %lf.\n", mean);
    return mean;
}

/*
 * Big thanks to https://rosettacode.org/wiki/Polynomial_regression#C 
 */
void polynomialfit(enum ac_states s) {
    gsl_multifit_linear_workspace *ws;
    gsl_matrix *cov, *X;
    gsl_vector *y, *c;
    double chisq;
    int i, j;
    
    X = gsl_matrix_alloc(SIZE_POINTS, DEGREE);
    y = gsl_vector_alloc(SIZE_POINTS);
    c = gsl_vector_alloc(DEGREE);
    cov = gsl_matrix_alloc(DEGREE, DEGREE);
    
    for(i=0; i < SIZE_POINTS; i++) {
        for(j=0; j < DEGREE; j++) {
            gsl_matrix_set(X, i, j, pow(i, j));
        }
        gsl_vector_set(y, i, conf.regression_points[s][i]);
    }
    
    ws = gsl_multifit_linear_alloc(SIZE_POINTS, DEGREE);
    gsl_multifit_linear(X, y, c, cov, &chisq, ws);
    
    /* store results */
    for(i = 0; i < DEGREE; i++) {
        state.fit_parameters[s][i] = gsl_vector_get(c, i);
    }
    DEBUG("%s curve: y = %lf + %lfx + %lfx^2\n", s == 0 ? "AC" : "BATT", state.fit_parameters[s][0], 
          state.fit_parameters[s][1], state.fit_parameters[s][2]);
    
    gsl_multifit_linear_free(ws);
    gsl_matrix_free(X);
    gsl_matrix_free(cov);
    gsl_vector_free(y);
    gsl_vector_free(c);
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
