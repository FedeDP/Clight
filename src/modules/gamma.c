#include "bus.h"
#include "utils.h"

#define GAMMA_LONG_TRANS_TIMEOUT 10         // 10s between each step with slow transitioning

static void receive_waiting_daytime(const msg_t *const msg, UNUSED const void* userdata);
static void receive_paused(const msg_t *const msg, UNUSED const void* userdata);
static void publish_temp_upd(int temp, int smooth, int step, int timeout);
static int parse_bus_reply(sd_bus_message *reply, const char *member, void *userdata);
static void set_temp(int temp, const time_t *now, int smooth, int step, int timeout);
static void ambient_callback(bl_upd *up);
static void on_new_next_dayevt(void);
static void on_daytime_req(void);
static void interface_callback(temp_upd *req);
static int on_temp_changed(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static void pause_mod(bool pause, enum mod_pause reason);

static sd_bus_slot *slot;
static bool long_transitioning, should_sync_temp;
static const self_t *daytime_ref;

MODULE_WITH_PAUSE("GAMMA");

static void init(void) {
    m_ref("DAYTIME", &daytime_ref);
    M_SUB(BL_UPD);
    M_SUB(TEMP_REQ);
    M_SUB(DAYTIME_UPD);
    M_SUB(NEXT_DAYEVT_UPD);
    M_SUB(SUSPEND_UPD);
    m_become(waiting_daytime);
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
            m_poisonpill(self());
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
        ambient_callback(up);
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

static void ambient_callback(bl_upd *up) {
    if (conf.gamma_conf.ambient_gamma) {
        /* Only account for target backlight changes, ie: not step ones */
        if (up->smooth || conf.bl_conf.no_smooth) {
            /* 
            * Note that conf.temp is not constant (it can be changed through bus api),
            * thus we have to always compute these ones.
            */
            const int diff = abs(conf.gamma_conf.temp[DAY] - conf.gamma_conf.temp[NIGHT]);
            const int min_temp = conf.gamma_conf.temp[NIGHT] < conf.gamma_conf.temp[DAY] ? 
                                conf.gamma_conf.temp[NIGHT] : conf.gamma_conf.temp[DAY]; 
            
            const int ambient_temp = (diff * up->new) + min_temp;
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

static void interface_callback(temp_upd *req) {
    if (req->new != conf.gamma_conf.temp[req->daytime]) {
        conf.gamma_conf.temp[req->daytime] = req->new;
        if (!conf.gamma_conf.ambient_gamma && req->daytime == state.day_time) {
            set_temp(req->new, NULL, req->smooth, req->step, req->timeout); // force refresh (passing NULL time_t*)
        }
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
