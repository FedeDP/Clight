#include "../inc/brightness.h"

static int check_err(int r, sd_bus_error err);
static void get_max_brightness(void);
static void get_current_brightness(void);
static void free_bus_structs(sd_bus_error *err, sd_bus_message *m);

struct brightness {
    int current;
    int max;
    int old;
};

static struct brightness br;
static sd_bus *bus;

void init_brightness(void) {
    int r;

    /* Connect to the system bus */
    r = sd_bus_open_system(&bus);
    if (r < 0) {
        fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
        state.quit = 1;
        return;
    }
    
    get_max_brightness();
    get_current_brightness();
    
    /* Initialize our brightness values array */
    if (!(state.values = calloc(conf.num_captures, sizeof(double)))) {
        state.quit = 1;
    }
}

static int check_err(int r, sd_bus_error err) {
    if (r < 0) {
        fprintf(stderr, "%s\n", err.message);
        /* Don't leave for ebusy errors */
        if (r != -EBUSY) {
            state.quit = 1;
        }
    }
    return r < 0;
}

static void get_max_brightness(void) {
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *m = NULL;
    int r;
    r = sd_bus_call_method(bus,
                           "org.clight.backlight",
                           "/org/clight/backlight",
                           "org.clight.backlight",
                           "getmaxbrightness",
                           &error,
                           &m,
                           "s",
                           conf.screen_path);
    if (check_err(r, error)) {
        goto finish;
    }
    
    /* Parse the response message */
    r = sd_bus_message_read(m, "i", &br.max);
    check_err(r, error);
    
finish:
    free_bus_structs(&error, m);
}

static void get_current_brightness(void) {
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *m = NULL;
    int r;
    r = sd_bus_call_method(bus,
                           "org.clight.backlight",
                           "/org/clight/backlight",
                           "org.clight.backlight",
                           "getbrightness",
                           &error,
                           &m,
                           "s",
                           conf.screen_path);
    if (check_err(r, error)) {
        goto finish;
    }
    
    /* Parse the response message */
    r = sd_bus_message_read(m, "i", &br.current);
    check_err(r, error);
    
finish:
    free_bus_structs(&error, m);
}

double set_brightness(double perc) {
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *m = NULL;
    int r;
    
    br.old = br.current;
    
    /* Issue the method call and store the response message in m */
    r = sd_bus_call_method(bus,
                           "org.clight.backlight",
                           "/org/clight/backlight",
                           "org.clight.backlight",
                           "setbrightness",
                           &error,
                           &m,
                           "si",
                           conf.screen_path,
                           (int) (br.max * perc));
    if (check_err(r, error)) {
        goto finish;
    }
    
    /* Parse the response message */
    r = sd_bus_message_read(m, "i", &br.current);
    if (!check_err(r, error)) {
        printf("New brightness value: %d\n", br.current);
    }
    
finish:
    free_bus_structs(&error, m);
    return (double)(br.current - br.old) / br.max;

}

double capture_frame(void) {
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *m = NULL;
    int r;
    double brightness = 0.0;
        
    /* Issue the method call and store the response message in m */
    r = sd_bus_call_method(bus,
                           "org.clight.backlight",
                           "/org/clight/backlight",
                           "org.clight.backlight",
                           "captureframe",
                           &error,
                           &m,
                           "s",
                           conf.dev_name);
    if (check_err(r, error)) {
        goto finish;
    }
    
    /* Parse the response message */
    r = sd_bus_message_read(m, "d", &brightness);
    check_err(r, error);
    
finish:
    free_bus_structs(&error, m);

    return brightness;
}

double compute_backlight(void) {
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


static void free_bus_structs(sd_bus_error *err, sd_bus_message *m) {
    if (err) {
        sd_bus_error_free(err);
    }
    
    if (m) {
        sd_bus_message_unref(m);
    }
}

void free_brightness(void) {
    sd_bus_flush_close_unref(bus);
    if (state.values) {
        free(state.values);
    }
}
