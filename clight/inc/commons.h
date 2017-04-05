#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/limits.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>
#include <poll.h>
#include <math.h>

enum modules { CAPTURE_IX, GAMMA_IX, LOCATION_IX, SIGNAL_IX, DPMS_IX, MODULES_NUM };

enum states { DAY, NIGHT, EVENT, UNKNOWN, SIZE };

struct config {
    int num_captures;
    int single_capture_mode;
    int timeout[SIZE]; // sizeof enum states
    char dev_name[PATH_MAX + 1];
    char screen_path[PATH_MAX + 1];
    int temp[SIZE]; // sizeof enum states DAY, NIGHT only
    int smooth_transition;
    double lat;
    double lon;
};

struct state {
    int quit;
    double *values;
    enum states time; // whether it is day or night time
    time_t next_event;
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
