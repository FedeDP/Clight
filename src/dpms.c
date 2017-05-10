#ifdef DPMS_PRESENT

#include "../inc/dpms.h"
#include <xcb/dpms.h>

static void init(void);
static int check(void);
static void destroy(void);

static xcb_connection_t *connection;
static struct self_t self = {
    .name = "Dpms",
    .idx = DPMS,
};

void set_dpms_self(void) {
    modules[self.idx].self = &self;
    modules[self.idx].init = init;
    modules[self.idx].check = check;
    modules[self.idx].destroy = destroy;
    set_self_deps(&self);
}

/*
 * Checks through xcb if DPMS is enabled for this xscreen
 */
static void init(void) {    
    connection = xcb_connect(NULL, NULL);

    if (connection && !xcb_connection_has_error(connection)) {
        xcb_dpms_info_cookie_t cookie = xcb_dpms_info(connection);
        xcb_dpms_info_reply_t *info = xcb_dpms_info_reply(connection, cookie, NULL);
        if (info) {
            int s = info->state;
            free(info);
            if (s) {
                // avoid polling this
                init_module(DONT_POLL, self.idx, NULL);
                /* start dependend modules (BRIGHTNESS) */
                return poll_cb(self.idx);
            }
        }
    }
    /*
     * if any error happens or info->state == 0 it means
     * dpms is disabled. Disable this module.
     */
    DEBUG("It seems dpms is disabled.\n");
    disable_module(self.idx);
}

static int check(void) {
    return conf.single_capture_mode || !getenv("XDG_SESSION_TYPE") || strcmp(getenv("XDG_SESSION_TYPE"), "x11");
}

static void destroy(void) {
    if (connection) {
        xcb_disconnect(connection);
    }
}

/*
 * info->power_level is one of:
 * DPMS Extension Power Levels
 * 0     DPMSModeOn          In use
 * 1     DPMSModeStandby     Blanked, low power
 * 2     DPMSModeSuspend     Blanked, lower power
 * 3     DPMSModeOff         Shut off, awaiting activity
 */
int get_screen_dpms(void) {
    int ret = -1;

    if (modules[self.idx].inited) {
        xcb_dpms_info_cookie_t cookie;
        xcb_dpms_info_reply_t *info;

        cookie = xcb_dpms_info(connection);
        info = xcb_dpms_info_reply(connection, cookie, NULL);

        if (info) {
            ret = info->power_level;
            free(info);
        }
    }
    return ret;
}

#endif
