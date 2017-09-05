#include "../inc/weather.h"
#include "../inc/bus.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define BUFFER_SIZE 2048

static void init(void);
static int check(void);
static void callback(void);
static void destroy(void);
static int get_weather(void);
static void upower_callback(const void *ptr);

static struct dependency dependencies[] = { {HARD, LOCATION}, {SOFT, BUS}, {SOFT, UPOWER} };
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
    struct bus_cb upower_cb = { UPOWER, upower_callback };
    int fd = start_timer(CLOCK_BOOTTIME, 0, 1);
    INIT_MOD(fd, &upower_cb);
}

static int check(void) {
    return !strlen(conf.weather_apikey);
}

static void callback(void) {
    if (!get_weather() && is_inited(BRIGHTNESS)) {
//         reset_timer(main_p[BRIGHTNESS].fd, conf.timeout[state.ac_state][old_state], conf.timeout[state.ac_state][state.time]);
    }
    set_timeout(conf.weather_timeout[state.ac_state], 0, main_p[self.idx].fd, 0);
}

static void destroy(void) {
    
}

static int get_weather(void) {
    char header[500] = {0};
    const char *provider = "api.openweathermap.org";
    const char *endpoint = "/data/2.5/weather";
    snprintf(header, sizeof(header),"GET %s?lat=%lf&lon=%lf&units=metric&APPID=%s HTTP/1.0\r\n"
                                    "Host: %s\r\n\r\n", 
                                    endpoint, conf.lat, conf.lon, conf.weather_apikey, provider);
    
    /* get host info */
    struct addrinfo hints = {0}, *res;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int err = getaddrinfo(provider, "80", &hints, &res);
    if (err) {
        WARN("Error getting address info: %s\n", gai_strerror(err));
        return -1;
    }
    
    /* Create socket and connect */
    int sockfd;
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
        return -1;
    }
    
    /* Send request */
    if (send(sockfd, header, sizeof(header), 0) == -1) {
        WARN("Error sending GET: %s\n", strerror(errno));
        return -1;
    }
    
    /* Receive data */
    char buf[BUFFER_SIZE] = {0};
    if (recv(sockfd, buf, sizeof(buf) - 1, 0) == -1) {
        WARN("Error receiving data: %s\n", strerror(errno));
        return -1;
    }
    
    if (!strstr(buf, "\"clouds\"")) {
        WARN("Json does not contain clouds information.\n");
        return -1;
    }
    
    sscanf(strstr(buf, "\"clouds\""), "\"clouds\":{\"all\":%d}", &state.cloudiness);
    DEBUG("Cloudiness: %d\n", state.cloudiness);
    close(sockfd);
    return 0;
}

static void upower_callback(const void *ptr) {
    int old_ac_state = *(int *)ptr;
    /* Force check that we received an ac_state changed event for real */
    if (old_ac_state != state.ac_state) {
        reset_timer(main_p[self.idx].fd, conf.weather_timeout[old_ac_state], conf.weather_timeout[state.ac_state]);
    }
}

unsigned int get_weather_aware_timeout(const int timeout) {
    if (state.cloudiness > 50) {
        return timeout * (1 - (double)(state.cloudiness - 50) / 100);
    }
    return timeout;
}
