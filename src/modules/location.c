#include <bus.h>
#include <my_math.h>
#include <interface.h>

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
static struct dependency dependencies[] = {
    {HARD, BUS}     // we need a bus connection
};
static struct self_t self = {
    .num_deps = SIZE(dependencies),
    .deps =  dependencies
};

MODULE(LOCATION);

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
        if (ret == 0) {
            /* We have a location, but no geoclue instance is running. */
            change_dep_type(GAMMA, self.idx, SOFT);
        }
        fd = DONT_POLL_W_ERR;
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
    int ret = -1;
    FILE *f = fopen(cache_file, "r");
    if (f) {
        if (fscanf(f, "%lf %lf\n", &state.current_loc.lat, &state.current_loc.lon) == 2) {
            emit_prop("Location");
            INFO("Location %.2lf %.2lf loaded from cache file!\n", state.current_loc.lat, state.current_loc.lon);
            ret = 0;
        }
        fclose(f);
    }
    if (ret != 0) {
        WARN("Error loading location from cache file.\n");
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
        WARN("Geoclue2 appears to be unsupported.\n");
    }
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
        slot = sd_bus_slot_unref(slot);
    }
}

static int check(void) {
    /*
     * If sunrise and sunset times, or lat and lon, are both passed,
     * disable LOCATION (but not gamma, by setting a SOFT dep instead of HARD)
     */
    memcpy(&state.current_loc, &conf.loc, sizeof(struct location));
    if ((strlen(conf.events[SUNRISE]) && strlen(conf.events[SUNSET])) || (conf.loc.lat != 0.0 && conf.loc.lon != 0.0)) {
        change_dep_type(GAMMA, self.idx, SOFT);
        return 1;
    }
    return 0;
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
    FILL_MATCH_DATA(state.current_loc);

    const char *new_location, *old_location;
    sd_bus_message_read(m, "oo", &old_location, &new_location);

    SYSBUS_ARG(lat_args, "org.freedesktop.GeoClue2", new_location, "org.freedesktop.GeoClue2.Location", "Latitude");
    SYSBUS_ARG(lon_args, "org.freedesktop.GeoClue2", new_location, "org.freedesktop.GeoClue2.Location", "Longitude");
    int r = get_property(&lat_args, "d", &state.current_loc.lat) + get_property(&lon_args, "d", &state.current_loc.lon);
    if (!r) {
        INFO("New location received: %.2lf, %.2lf\n", state.current_loc.lat, state.current_loc.lon);
        emit_prop("Location");
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

    /* It now needs proper /usr/share/applications/clightc.desktop name */
    int r = set_property(&id_args, 's', "clightc");
    if (!r) {
        unsigned int loc_thrs = LOC_DISTANCE_THRS;
        r = set_property(&thres_args, 'u', &loc_thrs); // 50kms
    }
    if (!r) {
        r = call(NULL, "", &call_args, NULL);
    }
    return r;
}

/*
 * Stop geoclue2 client.
 */
static void geoclue_client_stop(void) {
    SYSBUS_ARG(args, "org.freedesktop.GeoClue2", client, "org.freedesktop.GeoClue2.Client", "Stop");
    call(NULL, "", &args, NULL);
}

static void cache_location(void) {
    if (state.current_loc.lat != 0.0 && state.current_loc.lon != 0.0) {
        FILE *f = fopen(cache_file, "w");
        if (f) {
            fprintf(f, "%lf %lf\n", state.current_loc.lat, state.current_loc.lon);
            DEBUG("Latest location stored in cache file!\n");
            fclose(f);
        } else {
            WARN("Caching location: %s\n", strerror(errno));
        }
    }
}
