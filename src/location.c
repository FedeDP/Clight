#include "../inc/location.h"

static void init(void);
static int check(void);
static void destroy(void);
static void callback(void);
static int load_cache_location(void);
static void init_cache_file(void);
static int geoclue_init(void);
static int geoclue_get_client(void);
static int geoclue_hook_update(void);
static int on_geoclue_new_location(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int geoclue_client_start(void);
static void geoclue_client_stop(void);
static void cache_location(void);

static sd_bus_slot *slot;
static char client[PATH_MAX + 1], cache_file[PATH_MAX + 1];
static struct dependency dependencies[] = { {HARD, BUS} };
static struct self_t self = {
    .name = "Location",
    .idx = LOCATION,
    .num_deps = SIZE(dependencies),
    .deps =  dependencies
};

void set_location_self(void) {
    SET_SELF();
}

/*
 * init location:
 * init geoclue and set a match on bus on new location signal
 * if geoclue is not present, go ahead and try to load a location
 * from cache file. If it fails, DONT_POLL_W_ERR this module (ie: disable it),
 * else, DONT_POLL this module and consider it started.
 */
static void init(void) {
    int r = geoclue_init();
    /* 
     * timeout after 3s to check if geoclue2 gave us 
     * any location. Otherwise, attempt to load it from cache
     */
    int fd;
    init_cache_file();
    if (r == 0) {
        fd = start_timer(CLOCK_MONOTONIC, 3, 0);
    } else {
        int ret = load_cache_location();
        fd = ret == 0 ? DONT_POLL : DONT_POLL_W_ERR;
    }
    /* In case of errors, geoclue_init returns -1 -> disable location. */
    INIT_MOD(fd);
}

static void callback(void) {
    uint64_t t;
    if (read(main_p[self.idx].fd, &t, sizeof(uint64_t)) != -1) {
        load_cache_location();
    } else {
        /* Disarm timerfd as we received a location before it triggered */
        set_timeout(0, 0, main_p[self.idx].fd, 0);
    }
    /* disable this poll_cb */
    modules[self.idx].poll_cb = NULL;
}

static int load_cache_location(void) {
    int ret;
    FILE *f = fopen(cache_file, "r");
    if (f) {
        fscanf(f, "%lf %lf", &conf.lat, &conf.lon);
        INFO("Location %.2lf %.2lf loaded from cache file!\n", conf.lat, conf.lon);
        fclose(f);
        ret = 0;
    } else {
        DEBUG("Loading loc from cache file: %s\n", strerror(errno));
        ret = -1;
    }
    return ret;
}

static void init_cache_file(void) {
    if (getenv("XDG_CACHE_HOME")) {
        snprintf(cache_file, PATH_MAX, "%s/clight", getenv("XDG_CACHE_HOME"));
    } else {
        snprintf(cache_file, PATH_MAX, "%s/.cache/clight", getpwuid(getuid())->pw_dir);
    }
}

/*
 * Init geoclue, then checks if a location is already available.
 */
static int geoclue_init(void) {    
    int r = geoclue_get_client();
    if (r < 0) {
        goto end;
    }
    r = geoclue_hook_update();
    if (r < 0) {
        goto end;
    }
    r = geoclue_client_start();

end:
    /* In case of geoclue2 error, do not leave. Just disable this module */
    return -(r < 0);  // - 1 on error
}

/*
 * Stop geoclue2 client and store latest location to cache.
 */
static void destroy(void) {
    if (strlen(client)) {
        geoclue_client_stop();
    }
    cache_location();
    /* Destroy this match slot */
    if (slot) {
        sd_bus_slot_unref(slot);
    }
}

static int check(void) {
    /* 
     * If sunrise and sunset times, or lat and lon, are both passed, 
     * disable LOCATION (but not gamma, by setting a SOFT dep instead of HARD) 
     */
    if ((strlen(conf.events[SUNRISE]) && strlen(conf.events[SUNSET])) || (conf.lat != 0.0 && conf.lon != 0.0)) {
        change_dep_type(GAMMA, self.idx, SOFT);
        return 1;
    }
    return is_disabled(GAMMA);
}

/*
 * Store Client object path in client (static) global var
 */
static int geoclue_get_client(void) {
    struct bus_args args = {"org.freedesktop.GeoClue2", "/org/freedesktop/GeoClue2/Manager", "org.freedesktop.GeoClue2.Manager", "GetClient"};
    return bus_call(client, "o", &args, "");
}

/*
 * Hook our geoclue_new_location callback to PropertiesChanged dbus signals on GeoClue2 service.
 */
static int geoclue_hook_update(void) {
    struct bus_args args = {"org.freedesktop.GeoClue2", client, "org.freedesktop.GeoClue2.Client", "LocationUpdated" };
    return add_match(&args, &slot, on_geoclue_new_location);
}

/*
 * On new location callback: retrieve new_location object,
 * then retrieve latitude and longitude from that object and store them in our conf struct.
 */
static int on_geoclue_new_location(sd_bus_message *m, __attribute__((unused)) void *userdata, __attribute__((unused)) sd_bus_error *ret_error) {
    if (userdata) {
        *(int *)userdata = self.idx;
    }
    
    const char *new_location, *old_location;
    sd_bus_message_read(m, "oo", &old_location, &new_location);

    struct bus_args lat_args = {"org.freedesktop.GeoClue2", new_location, "org.freedesktop.GeoClue2.Location", "Latitude"};
    struct bus_args lon_args = {"org.freedesktop.GeoClue2", new_location, "org.freedesktop.GeoClue2.Location", "Longitude"};

    get_property(&lat_args, "d", &conf.lat);
    get_property(&lon_args, "d", &conf.lon);
    
    INFO("New location received: %.2lf, %.2lf\n", conf.lat, conf.lon);
    return 0;
}

/*
 * Start our geoclue2 client after having correctly set needed properties.
 */
static int geoclue_client_start(void) {
    struct bus_args call_args = {"org.freedesktop.GeoClue2", client, "org.freedesktop.GeoClue2.Client", "Start"};
    struct bus_args id_args = {"org.freedesktop.GeoClue2", client, "org.freedesktop.GeoClue2.Client", "DesktopId"};
    struct bus_args thres_args = {"org.freedesktop.GeoClue2", client, "org.freedesktop.GeoClue2.Client", "DistanceThreshold"};

    set_property(&id_args, 's', "clight");
    set_property(&thres_args, 'u', "50000"); // 50kms
    return bus_call(NULL, "", &call_args, "");
}

/*
 * Stop geoclue2 client.
 */
static void geoclue_client_stop(void) {
    struct bus_args args = {"org.freedesktop.GeoClue2", client, "org.freedesktop.GeoClue2.Client", "Stop"};
    bus_call(NULL, "", &args, "");
}

static void cache_location(void) {
    if (strlen(cache_file) && conf.lat != 0.0 && conf.lon != 0.0) {
        FILE *f = fopen(cache_file, "w");
        if (f) {
            fprintf(f, "%lf %lf\n", conf.lat, conf.lon);
            DEBUG("Latest location stored in cache file!\n");
            fclose(f);
        } else {
            DEBUG("Storing loc to cache file: %s\n", strerror(errno));
        }
    }
}
