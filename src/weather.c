#include "../inc/weather.h"
#include "../inc/bus.h"
#include "../inc/network.h"
#include "../inc/location.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define BUFFER_SIZE 2048

static void init(void);
static int check(void);
static void callback(void);
static void destroy(void);
static int get_weather(void);
static unsigned int get_weather_aware_timeout(int timeout);
static void upower_callback(const void *ptr);
static void network_callback(const void *ptr);
static void location_callback(const void *ptr);

/*
 * network with continuous disconnections can lead to
 * lots of owm weather api calls (and battery/network usage too).
 * So, we store last time an api call was made and avoid doing the same call
 * until conf.weather_timeout[state.ac_state] has really elapsed.
 */
static time_t last_call;
static int brightness_timeouts[SIZE_AC][SIZE_STATES];
static struct dependency dependencies[] = { {HARD, LOCATION}, {SOFT, UPOWER}, {SOFT, NETWORK} };
static struct self_t self = {
    .name = "Weather",
    .idx = WEATHER,
    .num_deps = SIZE(dependencies),
    .deps =  dependencies
};

void set_weather_self(void) {
    SET_SELF();
}

static void init(void) {
    /* Store correct brightness timeouts */
    memcpy(brightness_timeouts, conf.timeout, sizeof(int) * SIZE_AC * SIZE_STATES);

    struct bus_cb upower_cb = { UPOWER, upower_callback };
    struct bus_cb network_cb = { NETWORK, network_callback };
    struct bus_cb location_cb = { LOCATION, location_callback };

    int fd = start_timer(CLOCK_BOOTTIME, 0, network_enabled(state.nmstate));
    INIT_MOD(fd, &upower_cb, &network_cb, &location_cb);
}

static int check(void) {
    return !strlen(conf.weather_apikey);
}

static void callback(void) {
    uint64_t t;
    read(main_p[self.idx].fd, &t, sizeof(uint64_t));

    /* Store latest call time */
    time(&last_call);

    /*
     * get_weather returns 0 only if new cloudiness
     * is different from old cloudiness, thus we can be sure
     * we must update all brightness timeouts and reset BRIGHTNESS timer
     */
    if (!get_weather()) {
        /* Store old timeout */
        unsigned int old_timeout = conf.timeout[state.ac_state][state.time];
        /* Modify all brightness timeouts accounting for new cloudiness */
        for (int i = 0; i < SIZE_AC; i++) {
            for (int j = 0; j < SIZE_STATES; j++) {
                conf.timeout[i][j] = get_weather_aware_timeout(brightness_timeouts[i][j]);
            }
        }
        /* If brightness is inited */
        if (is_inited(BRIGHTNESS)) {
            reset_timer(main_p[BRIGHTNESS].fd, old_timeout, conf.timeout[state.ac_state][state.time]);
        }
    }
    set_timeout(conf.weather_timeout[state.ac_state], 0, main_p[self.idx].fd, 0);
}

static void destroy(void) {
    /* Skeleton function needed for modules interface */
}

/*
 * Calls through C socket openweathermap api to retrieve Cloudiness
 * for user position.
 * Returns -1 on error, 1 if new cloudiness is same as old cloudiness, 0 otherwise.
 */
static int get_weather(void) {
    int ret = -1, sockfd = -1;
    char header[500] = {0};
    const char *provider = "api.openweathermap.org";
    const char *endpoint = "/data/2.5/weather";
    snprintf(header, sizeof(header),"GET %s?lat=%lf&lon=%lf&units=metric&APPID=%s HTTP/1.0\r\n"
             "Host: %s\r\n\r\n",
             endpoint, conf.loc.lat, conf.loc.lon, conf.weather_apikey, provider);

    /* get host info */
    struct addrinfo hints = {0}, *res = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int err = getaddrinfo(provider, "80", &hints, &res);
    if (err) {
        WARN("Error getting address info: %s\n", gai_strerror(err));
        goto end;
    }

    /* Create socket and connect */
    for (struct addrinfo *rp = res; rp; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1) {
            continue;
        }
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1) {
            break;
        }
        WARN("Error connecting to socket: %s\n", strerror(errno));
        close(sockfd);
        sockfd = -1;
    }
    freeaddrinfo(res);

    if (sockfd == -1) {
        WARN("Error creating socket: %s\n", strerror(errno));
        goto end;
    }

    /* Send request */
    if (send(sockfd, header, sizeof(header), 0) == -1) {
        WARN("Error sending GET: %s\n", strerror(errno));
        goto end;
    }

    /* Receive data */
    char buf[BUFFER_SIZE] = {0};
    if (recv(sockfd, buf, sizeof(buf) - 1, 0) == -1) {
        WARN("Error receiving data: %s\n", strerror(errno));
        goto end;
    }

    if (!strstr(buf, "\"clouds\"")) {
        if (strstr(buf, "\"message\"")) {
            char *message = strstr(buf, "\"message\"") + strlen("\"message\"") + 3; // remove " \"" (space + opening ")
            char *ptr = strchr(message, '\"');
            if (ptr) {
                *ptr = '\n'; // remove closing " and put \n
                if (*(ptr + 1)) {
                    *(ptr + 1) = '\0'; // remove closing }
                }
            }
            WARN(message);
        } else {
            WARN("Json does not contain clouds information.\n");
        }
        goto end;
    }

    int old_cloudiness = state.cloudiness;
    sscanf(strstr(buf, "\"clouds\""), "\"clouds\":{\"all\":%d}", &state.cloudiness);
    INFO("Weather cloudiness: %d.\n", state.cloudiness);
    ret = old_cloudiness == state.cloudiness;

end:
    if (sockfd > 0) {
        close(sockfd);
    }
    return ret;
}

/*
 * Weather aware timeout goes from 0.99 timeout to 0.5 timeout
 */
static unsigned int get_weather_aware_timeout(int timeout) {
    if (state.cloudiness > 50) {
        timeout = timeout * (1 - (double)(state.cloudiness - 50) / 100);
    }
    return timeout;
}

static void upower_callback(const void *ptr) {
    int old_ac_state = *(int *)ptr;
    /* Force check that we received an ac_state changed event for real */
    if (old_ac_state != state.ac_state) {
        reset_timer(main_p[self.idx].fd, conf.weather_timeout[old_ac_state], conf.weather_timeout[state.ac_state]);
    }
}

static void network_callback(const void *ptr) {
    int old_network_state = *(int *)ptr;
    int enabled = network_enabled(state.nmstate);

    if (network_enabled(old_network_state) != enabled) {
        if (enabled) {
            /*
             * it means we had no network previously;
             * Restart now this module, paying attenction to latest time
             * a get_weather was called.
             */
            time_t now;
            time(&now);
            if (now - last_call > conf.weather_timeout[state.ac_state]) {
                set_timeout(0, 1, main_p[self.idx].fd, 0);
            } else {
                set_timeout(conf.weather_timeout[state.ac_state] - (now - last_call), 1, main_p[self.idx].fd, 0);
            }
            DEBUG("Weather module being restarted.\n");
        } else {
            /* We have not network now. Pause this module */
            set_timeout(0, 0, main_p[self.idx].fd, 0);
            DEBUG("Weather module being paused.\n");
        }
    }
}

/*
 * On location change,
 * reload weather for new location
 */
static void location_callback(__attribute__((unused)) const void *ptr) {
    struct location old_loc = *(struct location *)ptr;

    if (get_distance(old_loc, conf.loc) > LOC_DISTANCE_THRS) {
        set_timeout(0, 1, main_p[self.idx].fd, 0);
    }
}

