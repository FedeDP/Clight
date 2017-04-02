#include "../inc/brightness.h"

static void get_max_brightness(void);
static void get_current_brightness(void);

/*
 * Storage struct for our needed variables.
 */
struct brightness {
    int current;
    int max;
    int old;
};

static struct brightness br;

/*
 * Init brightness values (max and current)
 */
void init_brightness(void) {
    get_max_brightness();
    if (!state.quit) {
        get_current_brightness();

        /* Initialize our brightness values array */
        if (!state.quit && !(state.values = calloc(conf.num_captures, sizeof(double)))) {
            state.quit = 1;
        }
    }
}

static void get_max_brightness(void) {
    struct bus_args args = {"org.clight.backlight", "/org/clight/backlight", "org.clight.backlight", "getmaxbrightness"};

    bus_call(&br.max, "i", &args, "s", conf.screen_path);
}

static void get_current_brightness(void) {
    struct bus_args args = {"org.clight.backlight", "/org/clight/backlight", "org.clight.backlight", "getbrightness"};

    bus_call(&br.current, "i", &args, "s", conf.screen_path);
}

double set_brightness(double perc) {
    struct bus_args args = {"org.clight.backlight", "/org/clight/backlight", "org.clight.backlight", "setbrightness"};

    bus_call(&br.current, "i", &args, "si", conf.screen_path, (int) (br.max * perc));
    if (br.current != -1) {
        printf("New brightness value: %d\n", br.current);
    }
    return (double)(br.current - br.old) / br.max;
}

double capture_frame(void) {
    double brightness = 0.0;
    struct bus_args args = {"org.clight.backlight", "/org/clight/backlight", "org.clight.backlight", "captureframe"};

    bus_call(&brightness, "d", &args, "s", conf.dev_name);
    return brightness;
}

/*
 * Compute average captured frames brightness.
 * It will normalize data removing highest and lowest values.
 */
double compute_avg_brightness(void) {
    int lowest = 0, highest = 0;
    double total = 0.0;

    for (int i = 0; i < conf.num_captures; i++) {
        if (state.values[i] < state.values[lowest]) {
            lowest = i;
        } else if (state.values[i] > state.values[highest]) {
            highest = i;
        }

        total += state.values[i];
    }

    // total == 0.0 means every captured frame decompression failed
    if (total != 0.0 && conf.num_captures > 2) {
        // remove highest and lowest values to normalize
        total -= (state.values[highest] + state.values[lowest]);
        return total / (conf.num_captures - 2);
    }

    return total / conf.num_captures;
}

void free_brightness(void) {
    if (state.values) {
        free(state.values);
    }
}
