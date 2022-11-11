#include "interface.h"
#include "utils.h"

#define GAMMA_LONG_TRANS_TIMEOUT 10         // 10s between each step with slow transitioning

static void receive_waiting_daytime(const msg_t *const msg, UNUSED const void* userdata);
static void receive_paused(const msg_t *const msg, UNUSED const void* userdata);
static void publish_temp_upd(int temp, int smooth, int step, int timeout);
static int parse_bus_reply(sd_bus_message *reply, const char *member, void *userdata);
static void set_temp(int temp, const time_t *now, int smooth, int step, int timeout);
static void ambient_callback(bool smooth, double new);
static void on_new_next_dayevt(void);
static void on_daytime_req(void);
static void on_ambgamma_req(ambgamma_upd *up);
static void interface_callback(temp_upd *req);
static int on_temp_changed(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static void pause_mod(bool pause, enum mod_pause reason);
static int set_gamma(sd_bus *bus, const char *path, const char *interface, const char *property,
                     sd_bus_message *value, void *userdata, sd_bus_error *error);
static int set_ambgamma(sd_bus *bus, const char *path, const char *interface, const char *property,
                 sd_bus_message *value, void *userdata, sd_bus_error *error);

static int initial_temp;
static sd_bus_slot *slot;
static bool long_transitioning, should_sync_temp;
static const self_t *daytime_ref;
static const sd_bus_vtable conf_gamma_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_WRITABLE_PROPERTY("AmbientGamma", "b", NULL, set_ambgamma, offsetof(gamma_conf_t, ambient_gamma), 0),
    SD_BUS_WRITABLE_PROPERTY("NoSmooth", "b", NULL, NULL, offsetof(gamma_conf_t, no_smooth), 0),
    SD_BUS_WRITABLE_PROPERTY("TransStep", "i", NULL, NULL, offsetof(gamma_conf_t, trans_step), 0),
    SD_BUS_WRITABLE_PROPERTY("TransDuration", "i", NULL, NULL, offsetof(gamma_conf_t, trans_timeout), 0),
    SD_BUS_WRITABLE_PROPERTY("DayTemp", "i", NULL, set_gamma, offsetof(gamma_conf_t, temp[DAY]), 0),
    SD_BUS_WRITABLE_PROPERTY("NightTemp", "i", NULL, set_gamma, offsetof(gamma_conf_t, temp[NIGHT]), 0),
    SD_BUS_WRITABLE_PROPERTY("LongTransition", "b", NULL, NULL, offsetof(gamma_conf_t, long_transition), 0),
    SD_BUS_WRITABLE_PROPERTY("RestoreOnExit", "b", NULL, NULL, offsetof(gamma_conf_t, restore), 0),
    SD_BUS_VTABLE_END
};

DECLARE_MSG(temp_req, TEMP_REQ);
DECLARE_MSG(ambgamma_req, AMB_GAMMA_REQ);

API(Gamma, conf_gamma_vtable, conf.gamma_conf);
MODULE_WITH_PAUSE("GAMMA");

static void init(void) {
    m_ref("DAYTIME", &daytime_ref);
    M_SUB(BL_UPD);
    M_SUB(TEMP_REQ);
    M_SUB(AMB_GAMMA_REQ);
    M_SUB(DAYTIME_UPD);
    M_SUB(NEXT_DAYEVT_UPD);
    M_SUB(SUSPEND_UPD);
    m_become(waiting_daytime);
    
    // Store current temperature to later restore it if requested
    SYSBUS_ARG_REPLY(args, parse_bus_reply, &initial_temp, CLIGHTD_SERVICE, "/org/clightd/clightd/Gamma", "org.clightd.clightd.Gamma", "Get");
    if (call(&args, "ss", fetch_display(), fetch_env()) < 0) {
        // We are on an unsupported wayland compositor; kill ourself immediately without further message processing
        WARN("Failed to init. Killing module.\n");
        module_deregister((self_t **)&self());
    } else {
        init_Gamma_api();
    }
}

static bool check(void) {
    return true;
}

static bool evaluate(void) {
    return !conf.gamma_conf.disabled;
}

static void destroy(void) {
    if (slot) {
        slot = sd_bus_slot_unref(slot);
    }
    deinit_Gamma_api();
}

static void receive_waiting_daytime(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
    case DAYTIME_UPD: {
        if (module_is(daytime_ref, STOPPED)) {
            /*
             * We have been notified by LOCATION that neither
             * Geoclue (not installed) nor location cache file
             * could give us any location.
             */
            WARN("Killing GAMMA as no location provider is available.\n");
            module_deregister((self_t **)&self());
        } else {
            m_unbecome();
            
            SYSBUS_ARG(args, CLIGHTD_SERVICE, "/org/clightd/clightd/Gamma", "org.clightd.clightd.Gamma", "Changed");
            add_match(&args, &slot, on_temp_changed);
        }
        break;
    }
    default:
        break;
    }
}

static void receive(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
    case BL_UPD: {
        bl_upd *up = (bl_upd *)MSG_DATA();
        ambient_callback(up->smooth, up->new);
        break;
    }
    case TEMP_REQ: {
        temp_upd *up = (temp_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            if (msg->ps_msg->sender == daytime_ref) {
                on_daytime_req();
            } else {
                interface_callback(up);
            }
        }
        break;
    }
    case AMB_GAMMA_REQ: {
        ambgamma_upd *up = (ambgamma_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            on_ambgamma_req(up);
        }
        break;
    }
    case NEXT_DAYEVT_UPD:
        on_new_next_dayevt();
        break;
    case SUSPEND_UPD:
        pause_mod(state.suspended, SUSPEND);
        break;
    case SYSTEM_UPD:
        if (msg->ps_msg->type == LOOP_STOPPED && initial_temp && conf.gamma_conf.restore) {
            set_temp(initial_temp, NULL, false, 0, 0);
        }
        break;    
    default:
        break;
    }
}

static void receive_paused(const msg_t *const msg, UNUSED const void* userdata) {
    switch (MSG_TYPE()) {
    case TEMP_REQ: {
        /* 
         * We do not manage external temp_req; 
         * we just store that we should sync temp for clight internal temp requests.
         */
        temp_upd *up = (temp_upd *)MSG_DATA();
        if (VALIDATE_REQ(up)) {
            if (msg->ps_msg->sender == daytime_ref) {
                should_sync_temp = true;
            }
        }
        break;
    }
    case NEXT_DAYEVT_UPD:
        on_new_next_dayevt();
        break;
    case SUSPEND_UPD:
        pause_mod(state.suspended, SUSPEND);
        break;
    default:
        break;
    }
}

static void publish_temp_upd(int temp, int smooth, int step, int timeout) {
    DECLARE_HEAP_MSG(temp_msg, TEMP_UPD);
    temp_msg->temp.old = state.current_temp;
    temp_msg->temp.new = temp;
    temp_msg->temp.smooth = smooth;
    temp_msg->temp.step = step;
    temp_msg->temp.timeout = timeout;
    temp_msg->temp.daytime = state.day_time;
    M_PUB(temp_msg);
}

static int parse_bus_reply(sd_bus_message *reply, const char *member, void *userdata) {
    if (!strcmp(member, "Get")) {
        return sd_bus_message_read(reply, "i", userdata);
    }
    return sd_bus_message_read(reply, "b", userdata); 
}

static void set_temp(int temp, const time_t *now, int smooth, int step, int timeout) {
    int ok;
    SYSBUS_ARG_REPLY(args, parse_bus_reply, &ok, CLIGHTD_SERVICE, "/org/clightd/clightd/Gamma", "org.clightd.clightd.Gamma", "Set");
    
    /* Compute long transition steps and timeouts (if outside of event, fallback to normal transition) */
    if (conf.gamma_conf.long_transition && now && state.in_event) {
        smooth = 1;
        if (state.event_time_range == 0) {
            /* Remaining time in first half + second half of transition */
            timeout = (state.day_events[state.next_event] - *now) + conf.day_conf.event_duration;
            temp = conf.gamma_conf.temp[!state.day_time]; // use correct temp, ie the one for next event
        } else {
            /* Remaining time in second half of transition */
            timeout = conf.day_conf.event_duration - (*now - state.day_events[state.next_event]);
        }
        /* Temperature difference */
        step = abs(conf.gamma_conf.temp[DAY] - conf.gamma_conf.temp[NIGHT]);
        /* Compute each step size with a gamma_trans_timeout of 10s */
        step /= (((double)timeout) / GAMMA_LONG_TRANS_TIMEOUT);
        /* force gamma_trans_timeout to 10s (in ms) */
        timeout = GAMMA_LONG_TRANS_TIMEOUT * 1000;
        
        long_transitioning = true;
    } else {
        long_transitioning = false;
    }
        
    int r = call(&args, "ssi(buu)", fetch_display(), fetch_env(), temp, smooth, step, timeout);    
    if (!r && ok) {
        if (!long_transitioning && conf.gamma_conf.no_smooth) {
            INFO("%d gamma temp set.\n", temp);
            // we do not publish TEMP_UPD here as it will be published by on_temp_changed()
        } else {
            // publish target value and params for smooth temp change
            publish_temp_upd(temp, smooth, step, timeout);
            INFO("%s transition to %d gamma temp.\n", long_transitioning ? "Long" : "Normal", temp);
        }
    } else {
        WARN("Failed to set gamma temperature.\n");
    }
}

static void ambient_callback(bool smooth, double new) {
    if (conf.gamma_conf.ambient_gamma && !state.display_state) {
        /* Only account for target backlight changes, ie: not step ones */
        if (smooth || conf.bl_conf.smooth.no_smooth) {
            /* 
            * Note that conf.temp is not constant (it can be changed through bus api),
            * thus we have to always compute these ones.
            */
            const int diff = abs(conf.gamma_conf.temp[DAY] - conf.gamma_conf.temp[NIGHT]);
            const int min_temp = conf.gamma_conf.temp[NIGHT] < conf.gamma_conf.temp[DAY] ? 
                                conf.gamma_conf.temp[NIGHT] : conf.gamma_conf.temp[DAY]; 
            
            const int ambient_temp = (diff * new) + min_temp;
            set_temp(ambient_temp, NULL, !conf.gamma_conf.no_smooth, 
                    conf.gamma_conf.trans_step, conf.gamma_conf.trans_timeout); // force refresh (passing NULL time_t*)
        }
    }
}

static void on_new_next_dayevt(void) {    
    /* Properly reset long_transitioning when we change target event */
    if (long_transitioning) {
        INFO("Long transition ended.\n");
        long_transitioning = false;
    }
}

static void on_daytime_req(void) {
    /* 
     * Properly reset long_transitioning when we changed current day.
     * This is needed when we are suspended and then resumed with 
     * same next_event but in a different day/year (well this is kinda overkill)
     */
    static time_t last_t;
    
    const time_t t = time(NULL);
    struct tm tm_now, tm_old;
    localtime_r(&t, &tm_now);
    localtime_r(&last_t, &tm_old);
    
    if (long_transitioning &&
        (tm_now.tm_yday != tm_old.tm_yday || 
        tm_now.tm_year != tm_old.tm_year)) {
        
        on_new_next_dayevt();
    }
        
    last_t = t;
        
    if (!long_transitioning && !conf.gamma_conf.ambient_gamma) {
        set_temp(conf.gamma_conf.temp[state.day_time], &t, !conf.gamma_conf.no_smooth, 
                 conf.gamma_conf.trans_step, conf.gamma_conf.trans_timeout);
    }
}

static void on_ambgamma_req(ambgamma_upd *up) {
    conf.gamma_conf.ambient_gamma = up->new;
    if (!up->new) {
        // restore correct screen temp -> force refresh (passing NULL time_t*)
        // Note that long_transitioning cannot be true because we were in ambient gamma mode
        set_temp(conf.gamma_conf.temp[state.day_time], NULL, !conf.gamma_conf.no_smooth, 
                 conf.gamma_conf.trans_step, conf.gamma_conf.trans_timeout);
    } else {
        // Immediately set correct temp for current bl pct
        ambient_callback(true, state.current_bl_pct);
    }
}

static void interface_callback(temp_upd *req) {
    // req->new was already validated. Store it.
    conf.gamma_conf.temp[req->daytime] = req->new;
    if (!conf.gamma_conf.ambient_gamma && req->daytime == state.day_time) {
        /*
         * When not in ambient_gamma mode, 
         * if the requested daytime matches the current one,
         * force refresh current temp (passing NULL time_t*)
         */
        set_temp(req->new, NULL, req->smooth, req->step, req->timeout);
    } else {
        /*
         * Immediately set correct temp for current bl pct 
         * (given new temperature max/min values
         */
        ambient_callback(true, state.current_bl_pct);
    }
}

static int on_temp_changed(sd_bus_message *m, UNUSED void *userdata, UNUSED sd_bus_error *ret_error) {
   /* Only account for our display for Gamma.Changed signals */
    const char *display = NULL;
    sd_bus_message_read(m, "s", &display);
    if (own_display(display)) {
        // Publish each step for smooth changes
        int new_temp;
        sd_bus_message_read(m, "i", &new_temp);
        publish_temp_upd(new_temp, 0, abs(state.current_temp - new_temp), 0);
        state.current_temp = new_temp;
    }
    return 0;
}

static void pause_mod(bool pause, enum mod_pause reason) {
    if (CHECK_PAUSE(pause, reason, "GAMMA")) {
        if (pause) {
            m_become(paused);
        } else {
            m_unbecome();
            if (should_sync_temp) {
                should_sync_temp = false;
                on_daytime_req();
            }
        }
    }
}

static int set_gamma(sd_bus *bus, const char *path, const char *interface, const char *property,
                     sd_bus_message *value, void *userdata, sd_bus_error *error) {
    VALIDATE_PARAMS(value, "i", &temp_req.temp.new);
    
    temp_req.temp.daytime = userdata == &conf.gamma_conf.temp[DAY] ? DAY : NIGHT;
    temp_req.temp.smooth = -1; // use conf values
    M_PUB(&temp_req);
    return r;
}


int set_ambgamma(sd_bus *bus, const char *path, const char *interface, const char *property,
                 sd_bus_message *value, void *userdata, sd_bus_error *error) {
    VALIDATE_PARAMS(value, "b", &ambgamma_req.ambgamma.new);
    
    M_PUB(&ambgamma_req);
    return r;
}
