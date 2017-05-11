#include "../inc/location.h"

static void init(void);
static int check(void);
static void destroy(void);
static int geoclue_init(void);
static void geoclue_get_client(void);
static void geoclue_hook_update(void);
static int on_geoclue_new_location(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static void geoclue_client_start(void);
static void geoclue_client_stop(void);

static char client[PATH_MAX + 1];
static struct dependency dependencies[] = { {HARD, BUS} };
static struct self_t self = {
    .name = "Location",
    .idx = LOCATION,
    .num_deps = SIZE(dependencies),
    .deps =  dependencies
};

void set_location_self(void) {
    modules[self.idx].self = &self;
    modules[self.idx].init = init;
    modules[self.idx].check = check;
    modules[self.idx].destroy = destroy;
    set_self_deps(&self);
}

/*
 * init location:
 * init geoclue and set a match on bus on new location signal
 */
static void init(void) {
    int r = geoclue_init();
    /* In case of errors, geoclue_init returns -1 -> disable location. */
    init_module(r == 0 ? DONT_POLL : DONT_POLL_W_ERR, self.idx, NULL);
}

/*
 * Init geoclue, then checks if a location is already available.
 */
static int geoclue_init(void) {
    int r = 0;
    
    geoclue_get_client();
    if (state.quit) {
        goto end;
    }
    geoclue_hook_update();
    if (state.quit) {
        goto end;
    }
    geoclue_client_start();
    if (state.quit) {
        goto end;
    }

end:
    /* In case of geoclue2 error, do not leave. Just disable gamma support as geoclue2 is an opt-dep. */
    if (state.quit) {
        state.quit = 0; // do not leave
        r = -1;
    }
    return r;
}

/*
 * If we are using geoclue, stop client.
 */
static void destroy(void) {
    geoclue_client_stop();
}

static int check(void) {
    /* 
     * If sunrise and sunset times, or lat and lon, are both passed, 
     * disable LOCATION (but not gamma, by setting a SOFT dep instead of HARD) 
     */
    if ((strlen(conf.events[SUNRISE]) && strlen(conf.events[SUNSET])) || (conf.lat != 0.0 && conf.lon != 0.0)) {
        modules[GAMMA].self->deps[1].type = SOFT;
        return 1;
    }
    return conf.single_capture_mode || conf.no_gamma;
}

/*
 * Store Client object path in client (static) global var
 */
static void geoclue_get_client(void) {
    struct bus_args args = {"org.freedesktop.GeoClue2", "/org/freedesktop/GeoClue2/Manager", "org.freedesktop.GeoClue2.Manager", "GetClient"};
    bus_call(client, "o", &args, "");
}

/*
 * Hook our geoclue_new_location callback to PropertiesChanged dbus signals on GeoClue2 service.
 */
static void geoclue_hook_update(void) {
    struct bus_args args = {"org.freedesktop.GeoClue2", client, "org.freedesktop.GeoClue2.Client", "LocationUpdated" };
    add_match(&args, on_geoclue_new_location);
}

/*
 * On new location callback: retrieve new_location object,
 * then retrieve latitude and longitude from that object and store them in our conf struct.
 */
static int on_geoclue_new_location(sd_bus_message *m, __attribute__((unused)) void *userdata, __attribute__((unused)) sd_bus_error *ret_error) {
    const char *new_location, *old_location;

    sd_bus_message_read(m, "oo", &old_location, &new_location);

    struct bus_args lat_args = {"org.freedesktop.GeoClue2", new_location, "org.freedesktop.GeoClue2.Location", "Latitude"};
    struct bus_args lon_args = {"org.freedesktop.GeoClue2", new_location, "org.freedesktop.GeoClue2.Location", "Longitude"};

    get_property(&lat_args, "d", &conf.lat);
    get_property(&lon_args, "d", &conf.lon);
    
    /* Updated GAMMA module sunrise/sunset for new location */
    INFO("New location received: %.2lf, %.2lf\n", conf.lat, conf.lon);
    if (modules[GAMMA].inited) {
        state.events[SUNSET] = 0; // to force get_gamma_events to recheck sunrise and sunset for today
        set_timeout(0, 1, main_p[GAMMA].fd, 0);
    } else {
        /* if gamma was waiting for location, start it */
        poll_cb(self.idx);
    }
    return 0;
}

/*
 * Start our geoclue2 client after having correctly set needed properties.
 */
static void geoclue_client_start(void) {
    struct bus_args call_args = {"org.freedesktop.GeoClue2", client, "org.freedesktop.GeoClue2.Client", "Start"};
    struct bus_args id_args = {"org.freedesktop.GeoClue2", client, "org.freedesktop.GeoClue2.Client", "DesktopId"};
    struct bus_args thres_args = {"org.freedesktop.GeoClue2", client, "org.freedesktop.GeoClue2.Client", "DistanceThreshold"};

    set_property(&id_args, 's', "clight");
    set_property(&thres_args, 'u', "50000"); // 50kms
    bus_call(NULL, "", &call_args, "");
}

/*
 * Stop geoclue2 client.
 */
static void geoclue_client_stop(void) {
    struct bus_args args = {"org.freedesktop.GeoClue2", client, "org.freedesktop.GeoClue2.Client", "Stop"};
    bus_call(NULL, "", &args, "");
}
