#include "../inc/location.h"

#include <sys/eventfd.h>
#include <fcntl.h>

static int location_conf_init(void);
static int geoclue_init(void);
static void location_cb(void);
static void geoclue_check_initial_location(void);
static int is_geoclue(void);
static void geoclue_get_client(void);
static void geoclue_hook_update(void);
static int geoclue_new_location(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static void geoclue_client_start(void);
static void geoclue_client_stop(void);

static char client[PATH_MAX + 1];
static int inited;

/*
 * init location:
 * if lat and lon are passed as program cmdline args,
 * creates an eventfd and writes in it to let main poll
 * know that location is available thus gamma is ready to be set.
 *
 * Else, init geoclue support and returns sd_bus_get_fd to
 * let main_poll listen on new events from sd_bus.
 * Finally, checks if a location is already available through GeoClue2.
 *
 * Moreover, it stores a callback to be called on updated location event.
 */
void init_location(void) {    
    /* 
     * if sunrise/sunset times are passed through cmdline, 
     * or gamma support is disabled,
     * there is no need to load location module.
     */
    if (!conf.no_gamma && (!conf.events[SUNRISE] || !conf.events[SUNSET])) {
        int fd;
        
        if (conf.lat != 0 && conf.lon != 0) {
            fd = location_conf_init();
        } else {
            fd = geoclue_init();
        }
        set_pollfd(fd, LOCATION_IX, location_cb);
        INFO("Location module started.\n");
        inited = 1;
    }
}

/*
 * Creates eventfd and writes to it. Main poll will then wake up
 * and properly set gamma timer.
 */
static int location_conf_init(void) {
    // signal main_poll on this eventf that position is already available
    // as it is setted through a cmdline option
    int location_fd = eventfd(0, 0);
    if (location_fd == -1) {
        ERROR("%s\n", strerror(errno));
        state.quit = 1;
    } else {
        uint64_t value = 1;
        int r = write(location_fd, &value, sizeof(uint64_t));
        if (r == -1) {
            ERROR("%s\n", strerror(errno));
            state.quit = 1;
        }
    }
    return location_fd;
}

/*
 * Init geoclue, then process any old bus requests.
 * Checks if a location is already available and
 * finally returns sd_bus_get_fd to let main poll catch bus events.
 */
static int geoclue_init(void) {
    int location_fd = -1;

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
    // let main poll listen on new position events coming from geoclue
    location_fd = sd_bus_get_fd(bus);

    /* Process old requests -> otherwise our fd would get useless/wrong data */
    int r;
    do {
        r = sd_bus_process(bus, NULL);
    } while (r > 0);

    // emit a bus signal if there already is a location
    // this way it will be available to our main poll
    geoclue_check_initial_location();

end:
    /* In case of geoclue2 error, do not leave. Just disable gamma support as geoclue2 is an opt-dep. */
    if (state.quit) {
        ERROR("Error while loading geoclue2 support. Gamma correction tool disabled.\n");
        state.quit = 0;
        state.time = UNKNOWN;
        location_fd = -1;
    }
    return location_fd;
}

/*
 * When a new location is received, reset GAMMA timer to now + 1sec;
 * this way, check_gamma will be called and it will correctly set new timer.
 */
static void location_cb(void) {
    /* we received a new user position */
    if (!is_geoclue()) {
        uint64_t t;
        /* it is not from a bus signal as geoclue2 is not being used */
        read(main_p.p[LOCATION_IX].fd, &t, sizeof(uint64_t));
    } else {
        sd_bus_process(bus, NULL);
    }
    INFO("New location received: %.2lf, %.2lf\n", conf.lat, conf.lon);
    set_timeout(1, 0, main_p.p[GAMMA_IX].fd, 0);
}

/*
 * Checks if a location is already available through GeoClue2
 * (a LocationUpdated signal would not be sent until a real location update would happen.)
 * and emits a signal that we will receive in main_poll thanks to our match.
 */
static void geoclue_check_initial_location(void) {
    char loc_obj[PATH_MAX + 1] = {0};
    struct bus_args args = {"org.freedesktop.GeoClue2", client, "org.freedesktop.GeoClue2.Client", "Location"};

    get_property(&args, "o", loc_obj);
    if (strlen(loc_obj) > 0 && strcmp(loc_obj, "/")) {
        int r = sd_bus_emit_signal(bus, client, "org.freedesktop.GeoClue2.Client",
                            "LocationUpdated", "oo", loc_obj, loc_obj);
        check_err(r, NULL);
    }
}

/*
 * If we are using geoclue, stop client.
 */
void destroy_location(void) {
    if (inited) {
        if (is_geoclue()) {
            geoclue_client_stop();
        } else if (main_p.p[LOCATION_IX].fd > 0) {
            close(main_p.p[LOCATION_IX].fd);
        }
        INFO("Location module destroyed.\n");
    }
}

/*
 * Whether we are using geoclue (thus client object length is > 0)
 */
static int is_geoclue(void) {
    return strlen(client) > 0;
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
    struct bus_args args = {
        .path = client,
        .interface = "org.freedesktop.GeoClue2.Client",
        .member = "LocationUpdated"
    };
    add_match(&args, geoclue_new_location);
}

/*
 * On new location callback: retrieve new_location object,
 * then retrieve latitude and longitude from that object and store them in our conf struct.
 */
static int geoclue_new_location(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    const char *new_location, *old_location;

    sd_bus_message_read(m, "oo", &old_location, &new_location);

    struct bus_args lat_args = {"org.freedesktop.GeoClue2", new_location, "org.freedesktop.GeoClue2.Location", "Latitude"};
    struct bus_args lon_args = {"org.freedesktop.GeoClue2", new_location, "org.freedesktop.GeoClue2.Location", "Longitude"};

    get_property(&lat_args, "d", &conf.lat);
    get_property(&lon_args, "d", &conf.lon);
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
