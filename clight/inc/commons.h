#define __USE_XOPEN

#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/limits.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <stdarg.h>
#include <poll.h>
#include <math.h>

enum modules { CAPTURE_IX, GAMMA_IX, LOCATION_IX, SIGNAL_IX, DPMS_IX, MODULES_NUM };

enum states { DAY, NIGHT, EVENT, UNKNOWN, SIZE_STATES };

enum events { SUNRISE, SUNSET, SIZE_EVENTS };

struct config {
    int num_captures;
    int single_capture_mode;
    int timeout[SIZE_STATES]; // sizeof enum states
    char *dev_name;
    char *screen_path;
    int temp[SIZE_STATES]; // sizeof enum states DAY, NIGHT only
    int smooth_transition;
    double lat;
    double lon;
    char *events[2];
};

struct state {
    int quit;
    enum states time; // whether it is day or night time
    time_t events[SIZE_EVENTS]; // today events (sunrise/sunset)
    enum events next_event; // next event index
    int event_time_range;
};

struct poll_s {
    struct pollfd p[MODULES_NUM - 1];
    /*
     * pointer to poll callback function for a module
     */
    void (*cb[MODULES_NUM - 1])(void);
};

struct state state;
struct config conf;
int camera_width, camera_height;
uint8_t *buffer;
struct poll_s main_p; //cannot be array as poll() wants a &struct pollfd...
