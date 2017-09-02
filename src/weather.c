#include "../inc/weather.h"
#include "../inc/bus.h"
#include <curl/curl.h>
#include <jansson.h>

#define BUFFER_SIZE 2048

struct json_write_result {
    char data[BUFFER_SIZE];
    unsigned long position;
};

static void init(void);
static int check(void);
static void callback(void);
static void destroy(void);
static void get_weather(void);
static size_t write_data_buffer(void *ptr, size_t size, size_t nmemb, void *stream);
static int owm_fetch_remote(struct json_write_result *json);
static void parse_json(const char *json);
static void upower_callback(const void *ptr);

static CURL *handle;
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
    handle = curl_easy_init();
    if (handle) {
        struct bus_cb upower_cb = { UPOWER, upower_callback };
        int fd = start_timer(CLOCK_BOOTTIME, 0, 1);
        INIT_MOD(fd, &upower_cb);
    } else {
        WARN("Could not instantiate curl handle.\n");
        INIT_MOD(DONT_POLL_W_ERR);
    }
}

static int check(void) {
    return !strlen(conf.weather_apikey);
}

static void callback(void) {
    get_weather();
    set_timeout(conf.weather_timeout[state.ac_state], 0, main_p[self.idx].fd, 0);
}

static void destroy(void) {
    if (handle) {
        curl_easy_cleanup(handle);
    }
}

static void get_weather(void) {
    struct json_write_result json = {{0}};
    
    if (!owm_fetch_remote(&json)) {
        parse_json(json.data);
    }
}

/* Callback function for curl */
static size_t write_data_buffer(void *ptr, size_t size, size_t nmemb, void *stream) {
    struct json_write_result *result = (struct json_write_result * )stream;
    if (result->position + size * nmemb >= BUFFER_SIZE - 1) {
        fprintf(stderr, "Write Buffer is too small\n");
        return 0;
    }
    memcpy(result->data + result->position, ptr, size * nmemb);
    result->position += size * nmemb;
    return size * nmemb;
}

/* Fetch json from openweathermap */
static int owm_fetch_remote(struct json_write_result *json) {
    int ret = -1;
    CURLcode result;
    
    char url [512] = {0};
    const char *provider = "http://api.openweathermap.org/data/2.5/weather";
        
    snprintf(url, sizeof(url), "%s?lat=%lf&lon=%lf&units=metric&APPID=%s", provider, conf.lat, conf.lon, conf.weather_apikey);
        
    curl_easy_setopt(handle, CURLOPT_URL, url);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_data_buffer);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, json);
        
    result = curl_easy_perform(handle);
    if (result != CURLE_OK) {
        WARN("Failed to retrieve data (%s)\n", curl_easy_strerror(result));
    } else {
        ret = 0;
    }
    return ret;
}

/* Parse json with json_unpack (jansson library) */
static void parse_json(const char *json) {
    json_error_t error;
    json_t *root = json_loads(json, 0, &error);
    if (!root) {
        WARN("Error on line %d (%s)\n", error.line, error.text);
    } else {
        if (json_unpack(root, "{s:{s:i}}", "clouds", "all", &state.cloudiness)) {
            WARN("Failed to find cloudiness in json.\n");
        } else {
            DEBUG("Cloudiness: %d\n", state.cloudiness);
        }
    }
    json_decref(root);
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
