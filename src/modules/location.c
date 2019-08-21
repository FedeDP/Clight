#include <my_math.h>
#include <bus.h>

#define LOC_TIME_THRS 600                   // time threshold (seconds) before triggering location changed events (10mins)
#define LOC_DISTANCE_THRS 50000             // threshold for location distances before triggering location changed events (50km)

static int load_cache_location(void);
static void init_cache_file(void);
static int geoclue_init(void);
static int geoclue_get_client(void);
static int geoclue_hook_update(void);
static int on_geoclue_new_location(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int geoclue_client_start(void);
static void geoclue_client_stop(void);
static void cache_location(void);
static void publish_location(double old_lat, double old_lon);

static sd_bus_slot *slot;
static char client[PATH_MAX + 1], cache_file[PATH_MAX + 1];
static loc_upd loc_msg = { LOCATION_UPD };

const char *loc_topic = "Location";

MODULE("LOCATION");

static void init(void) {
    init_cache_file();
    int r = geoclue_init();
    if (r == 0) {
        /*
         * timeout after 3s to check if geoclue2 gave us
         * any location. Otherwise, attempt to load it from cache
         */
        int fd = start_timer(CLOCK_MONOTONIC, 3, 0);
        m_register_fd(fd, true, NULL);
    } else {
        WARN("LOCATION: Failed to init.\n");
        load_cache_location();
        m_poisonpill(self());
    }
}

static bool check(void) {
    return true;
}

static bool evaluate(void) {
    /* 
     * Only start when no location and no fixed times for both events are specified in conf
     * AND GAMMA is enabled
     */
    return  !conf.no_gamma && 
            (conf.loc.lat == LAT_UNDEFINED || conf.loc.lon == LON_UNDEFINED) && 
            (!strlen(conf.events[SUNRISE]) || !strlen(conf.events[SUNSET]));
}

/*
 * Stop geoclue2 client and store latest location to cache.
 */
static void destroy(void) {
    if (strlen(client)) {
        geoclue_client_stop();
        cache_location();
    }
    /* Destroy this match slot */
    if (slot) {
        slot = sd_bus_slot_unref(slot);
    }
}

static void receive(const msg_t *const msg, const void* userdata) {
    if (!msg->is_pubsub) {
        read_timer(msg->fd_msg->fd);
        if (state.current_loc.lat == LAT_UNDEFINED || state.current_loc.lon == LON_UNDEFINED) {
            load_cache_location();
        }
    }
}

static int load_cache_location(void) {
    int ret = -1;
    FILE *f = fopen(cache_file, "r");
    if (f) {
        if (fscanf(f, "%lf %lf\n", &state.current_loc.lat, &state.current_loc.lon) == 2) {
            publish_location(LAT_UNDEFINED, LON_UNDEFINED);
            INFO("LOCATION: %.2lf %.2lf loaded from cache file!\n", state.current_loc.lat, state.current_loc.lon);
            ret = 0;
        }
        fclose(f);
    }
    if (ret != 0) {
        WARN("LOCATION: Error loading from cache file.\n");
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
    if (r < 0) {
        WARN("LOCATION: Geoclue2 appears to be unsupported.\n");
    }
    /* In case of geoclue2 error, do not leave. Just disable this module */
    return -(r < 0);  // - 1 on error
}

/*
 * Store Client object path in client (static) global var
 */
static int geoclue_get_client(void) {
    SYSBUS_ARG(args, "org.freedesktop.GeoClue2", "/org/freedesktop/GeoClue2/Manager", "org.freedesktop.GeoClue2.Manager", "GetClient");
    return call(client, "o", &args, NULL);
}

/*
 * Hook our geoclue_new_location callback to PropertiesChanged dbus signals on GeoClue2 service.
 */
static int geoclue_hook_update(void) {
    SYSBUS_ARG(args, "org.freedesktop.GeoClue2", client, "org.freedesktop.GeoClue2.Client", "LocationUpdated");
    return add_match(&args, &slot, on_geoclue_new_location);
}

/*
 * On new location callback: retrieve new_location object,
 * then retrieve latitude and longitude from that object and store them in our conf struct.
 */
static int on_geoclue_new_location(sd_bus_message *m, void *userdata, __attribute__((unused)) sd_bus_error *ret_error) {
    /* Only if no conf location is set */
    if (conf.loc.lat == LAT_UNDEFINED && conf.loc.lon == LON_UNDEFINED) {
        const char *new_location, *old_location;
        sd_bus_message_read(m, "oo", &old_location, &new_location);

        const double old_lat = state.current_loc.lat;
        const double old_lon = state.current_loc.lon;
    
        SYSBUS_ARG(lat_args, "org.freedesktop.GeoClue2", new_location, "org.freedesktop.GeoClue2.Location", "Latitude");
        SYSBUS_ARG(lon_args, "org.freedesktop.GeoClue2", new_location, "org.freedesktop.GeoClue2.Location", "Longitude");
        int r = get_property(&lat_args, "d", &state.current_loc.lat, sizeof(state.current_loc.lat)) + 
                get_property(&lon_args, "d", &state.current_loc.lon, sizeof(state.current_loc.lon));
        if (!r) {
            INFO("LOCATION: New location received: %.2lf, %.2lf.\n", state.current_loc.lat, state.current_loc.lon);
            publish_location(old_lat, old_lon);
        }
    }
    return 0;
}

/*
 * Start our geoclue2 client after having correctly set needed properties.
 */
static int geoclue_client_start(void) {
    SYSBUS_ARG(call_args, "org.freedesktop.GeoClue2", client, "org.freedesktop.GeoClue2.Client", "Start");
    SYSBUS_ARG(id_args, "org.freedesktop.GeoClue2", client, "org.freedesktop.GeoClue2.Client", "DesktopId");
    SYSBUS_ARG(thres_args, "org.freedesktop.GeoClue2", client, "org.freedesktop.GeoClue2.Client", "DistanceThreshold");
    SYSBUS_ARG(time_args, "org.freedesktop.GeoClue2", client, "org.freedesktop.GeoClue2.Client", "TimeThreshold");

    /* It now needs proper /usr/share/applications/clightc.desktop name */
    set_property(&id_args, 's', "clightc");
    set_property(&time_args, 'u', &(unsigned int) { LOC_TIME_THRS });
    set_property(&thres_args, 'u', &(unsigned int) { LOC_DISTANCE_THRS });
    return call(NULL, "", &call_args, NULL);
}

/*
 * Stop geoclue2 client.
 */
static void geoclue_client_stop(void) {
    SYSBUS_ARG(args, "org.freedesktop.GeoClue2", client, "org.freedesktop.GeoClue2.Client", "Stop");
    call(NULL, "", &args, NULL);
}

static void cache_location(void) {
    if (state.current_loc.lat != LAT_UNDEFINED && state.current_loc.lon != LON_UNDEFINED) {
        FILE *f = fopen(cache_file, "w");
        if (f) {
            fprintf(f, "%lf %lf\n", state.current_loc.lat, state.current_loc.lon);
            DEBUG("LOCATION: Latest location stored in cache file!\n");
            fclose(f);
        } else {
            WARN("LOCATION: Caching location failed: %s.\n", strerror(errno));
        }
    }
}

static void publish_location(double old_lat, double old_lon) {
    loc_msg.old.lat = old_lat;
    loc_msg.old.lon = old_lon;
    loc_msg.new.lat = state.current_loc.lat;
    loc_msg.new.lon = state.current_loc.lon;
    M_PUB(loc_topic, &loc_msg);
}
