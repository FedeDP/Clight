#include "bus.h"

/**
 * LOCATION module will be deregistered if:
 *  * geoclue is not present
 *  * geoclue is present but fail to start correctly
 *  * geoclue is present and started correctly, but it is not responding (killed after GEOCLUE_TIMEOUT seconds)
 */

#define LOC_TIME_THRS   600                   // time threshold (seconds) before triggering location changed events (10mins)
#define GEOCLUE_TIMEOUT 30                    // timeout for waiting on geoclue to give us a position; otherwise kills module

typedef enum { GEOCLUE_NONE, GEOCLUE_PRESENT, GEOCLUE_STARTED, GEOCLUE_FAILED } geoclue_state;

static void fail_geoclue(geoclue_state st);
static void load_cache_location(void);
static void init_cache_file(void);
static int geoclue_init(void);
static int parse_bus_reply(sd_bus_message *reply, const char *member, void *userdata);
static int geoclue_ping(void);
static int geoclue_get_client(void);
static int geoclue_hook_update(void);
static int on_geoclue_new_location(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int geoclue_client_start(void);
static void geoclue_client_delete(void);
static void cache_location(void);
static void publish_location(double new_lat, double new_lon, message_t *l);

static sd_bus_slot *slot;
static int timeout_fd = -1;
static geoclue_state geoclue_st;
static char client[PATH_MAX + 1], cache_file[PATH_MAX + 1];

DECLARE_MSG(loc_msg, LOC_UPD);
DECLARE_MSG(loc_req, LOCATION_REQ);

MODULE("LOCATION");

static void init(void) {
    init_cache_file();
    M_SUB(LOCATION_REQ);
    
    // Give 30s of time to geoclue to give us a position before killing module
    timeout_fd = start_timer(CLOCK_BOOTTIME, GEOCLUE_TIMEOUT, 0);
    m_register_fd(timeout_fd, true, NULL);
}

static bool check(void) {
   return true;
}

static bool evaluate(void) {
    /* 
     * Only start when neither a location nor fixed times
     * for both events are specified in conf
     */
    return  !conf.wizard && 
            ((conf.day_conf.loc.lat == LAT_UNDEFINED || conf.day_conf.loc.lon == LON_UNDEFINED) && 
            (!strlen(conf.day_conf.day_events[SUNRISE]) || !strlen(conf.day_conf.day_events[SUNSET])));
}

/*
 * Stop geoclue2 client and store latest location to cache.
 */
static void destroy(void) {
    if (strlen(client)) {
        geoclue_client_delete();
    }
    /* Destroy this match slot */
    if (slot) {
        slot = sd_bus_slot_unref(slot);
    }
}

static void receive(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
    case FD_UPD:
        read_timer(msg->fd_msg->fd);
        /*
         * We had no cached location and geoclue client timed out;
         * tell other modules an go on dropping location
         */
        switch (geoclue_st) {
        case GEOCLUE_NONE:
            WARN("Failed to init (no geoclue2 present?). Killing module.\n");
            break;
        case GEOCLUE_PRESENT:
        case GEOCLUE_STARTED:
            WARN("Timed out waiting on location provided by Geoclue2. Killing module.\n");
            break;
        case GEOCLUE_FAILED:
            WARN("Failed to start Geoclue2 client. Killing module.\n");
            break;
        }

        if (state.current_loc.lat == LAT_UNDEFINED || state.current_loc.lon == LON_UNDEFINED) {
            publish_location(LAT_UNDEFINED, LON_UNDEFINED, &loc_msg);
        }
        
        // if we came here, just kill ourself
        module_deregister((self_t **)&self());
        break;
    case LOCATION_REQ: {
        loc_upd *l = (loc_upd *)MSG_DATA();
        if (VALIDATE_REQ(l)) {
            INFO("New location received: %.2lf, %.2lf.\n", l->new.lat, l->new.lon);
            // publish location before storing new location as state.current_loc is sent as "old" parameter
            publish_location(l->new.lat, l->new.lon, &loc_msg);
            memcpy(&state.current_loc, &l->new, sizeof(loc_t));
        }
        break;
    }
    case SYSTEM_UPD:
        if (msg->ps_msg->type == LOOP_STARTED) {
            load_cache_location();
            if (geoclue_init() != 0) {
                /*
                 * no cached location present and no geoclue; 
                 * fire immediately to take care of module cleanup and deregister
                 */
                fail_geoclue(GEOCLUE_NONE);
            } else {
                geoclue_st = GEOCLUE_PRESENT;
            }
        } else if (msg->ps_msg->type == LOOP_STOPPED) {
            cache_location();
        }
        break;
    default:
        break;
    }
}

static void fail_geoclue(geoclue_state st) {
    geoclue_st = st;
    set_timeout(0, 1, timeout_fd, 0);
}

static void load_cache_location(void) {
    FILE *f = fopen(cache_file, "r");
    if (f) {
        double new_lat, new_lon;
        if (fscanf(f, "%lf %lf\n", &new_lat, &new_lon) == 2) {
            publish_location(new_lat, new_lon, &loc_req);
            DEBUG("%.2lf %.2lf loaded from cache file.\n", new_lat, new_lon);
        } else {
            WARN("Could not load cached location, wrong format.\n");
        }
        fclose(f);
    } else {
        DEBUG("Could not find location cache file.\n");
    }
}

static void init_cache_file(void) {
    if (getenv("XDG_CACHE_HOME")) {
        snprintf(cache_file, PATH_MAX, "%s/clight", getenv("XDG_CACHE_HOME"));
    } else {
        snprintf(cache_file, PATH_MAX, "%s/.cache/clight", getpwuid(getuid())->pw_dir);
    }
}

static int geoclue_init(void) {
    // Check if geoclue is available, otherwise die 
    int r = geoclue_ping();
    if (r >= 0) {
        // GetClient is called async, and will later start the client in its callback, see parse_bus_reply
        r = geoclue_get_client();
        if (r < 0) {
            WARN("Geoclue2 is present but failed to give us a client.\n");
        }
    } else {
        DEBUG("Geoclue2 is not present.\n");
    }
    return -(r < 0);  // - 1 on error
}

static int parse_bus_reply(sd_bus_message *reply, const char *member, void *userdata) {
    int r = -EINVAL;
    /*
     * If self is null it means module was already deregistered;
     * may be geoclue took too long to respond and timeout_fd killed the module.
     * Note that "GetClient" answer is received async thus can be called even after
     * module was deregistered.
     * In this case, ignore the reply.
     */
    if (!strcmp(member, "Ping") || self() == NULL) {
        return 0;
    }
    if (!strcmp(member, "GetClient")) {
        const char *cl = NULL;
        r = sd_bus_message_read(reply, "o", &cl);
        if (r >= 0 && cl) {
            strncpy(client, cl, PATH_MAX);
            r = geoclue_hook_update();
            if (r >= 0) {
                r = geoclue_client_start();
            }
        }
        if (r < 0 || !cl) {
            fail_geoclue(GEOCLUE_FAILED);
        } else {
            geoclue_st = GEOCLUE_STARTED;
        }
    }
    return r;
}

static int geoclue_ping(void) {
    SYSBUS_ARG_REPLY(args, parse_bus_reply, NULL, "org.freedesktop.GeoClue2", "/org/freedesktop/GeoClue2", "org.freedesktop.DBus.Peer", "Ping");
    return call(&args, NULL);
}

/*
 * Store Client object path in client (static) global var
 */
static int geoclue_get_client(void) {
    // Make it async! We need a static lifetime object!
    static SYSBUS_ARG_REPLY(args, parse_bus_reply, NULL, "org.freedesktop.GeoClue2", "/org/freedesktop/GeoClue2/Manager", "org.freedesktop.GeoClue2.Manager", "GetClient");
    args.async = true;
    return call(&args, NULL);
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
static int on_geoclue_new_location(sd_bus_message *m, UNUSED void *userdata, UNUSED sd_bus_error *ret_error) {
    const char *new_location;
    sd_bus_message_read(m, "oo", NULL, &new_location);

    double new_lat, new_lon;
    
    SYSBUS_ARG(lat_args, "org.freedesktop.GeoClue2", new_location, "org.freedesktop.GeoClue2.Location", "Latitude");
    SYSBUS_ARG(lon_args, "org.freedesktop.GeoClue2", new_location, "org.freedesktop.GeoClue2.Location", "Longitude");
    int r = get_property(&lat_args, "d", &new_lat) + 
            get_property(&lon_args, "d", &new_lon);
    if (!r) {
        DEBUG("%.2lf %.2lf received from Geoclue2.\n", new_lat, new_lon);
        publish_location(new_lat, new_lon, &loc_req);
        if (timeout_fd != -1) {
            // disable timeout_fd as geoclue is responding!
            m_deregister_fd(timeout_fd);
            timeout_fd = -1;
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
    SYSBUS_ARG(accuracy_args, "org.freedesktop.GeoClue2", client, "org.freedesktop.GeoClue2.Client", "RequestedAccuracyLevel");

    /* It now needs proper /usr/share/applications/clightc.desktop name */
    int r = set_property(&id_args, "s", (uintptr_t)"clightc");
    set_property(&time_args, "u",  LOC_TIME_THRS);
    set_property(&thres_args, "u", LOC_DISTANCE_THRS * 1000);
    r += set_property(&accuracy_args, "u",  2); // https://www.freedesktop.org/software/geoclue/docs/geoclue-gclue-enums.html#GClueAccuracyLevel -> GCLUE_ACCURACY_LEVEL_CITY
    return r + call(&call_args, NULL);
}

/*
 * Stop and delete geoclue2 client.
 */
static void geoclue_client_delete(void) {
    SYSBUS_ARG(stop_args, "org.freedesktop.GeoClue2", client, "org.freedesktop.GeoClue2.Client", "Stop");
    call(&stop_args, NULL);

    SYSBUS_ARG(del_args, "org.freedesktop.GeoClue2", "/org/freedesktop/GeoClue2/Manager", "org.freedesktop.GeoClue2.Manager", "DeleteClient");
    call(&del_args, "o", client);
}

static void cache_location(void) {
    if (state.current_loc.lat != LAT_UNDEFINED && state.current_loc.lon != LON_UNDEFINED) {
        FILE *f = fopen(cache_file, "w");
        if (f) {
            fprintf(f, "%lf %lf\n", state.current_loc.lat, state.current_loc.lon);
            DEBUG("Latest location stored in cache file!\n");
            fclose(f);
        } else {
            WARN("Caching location failed: %s.\n", strerror(errno));
        }
    }
}

static void publish_location(double new_lat, double new_lon, message_t *l) {
    l->loc.old.lat = state.current_loc.lat;
    l->loc.old.lon = state.current_loc.lon;
    l->loc.new.lat = new_lat;
    l->loc.new.lon = new_lon;
    M_PUB(l);
}
