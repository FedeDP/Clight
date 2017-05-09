#ifdef DPMS_PRESENT

#include "../inc/dpms.h"
#include <xcb/dpms.h>
#include <stdlib.h>

static void init(void);
static void destroy(void);

static xcb_connection_t *connection;
static int dpms_enabled;

static struct self_t self = {
    .name = "Dpms",
    .idx = DPMS,
};

void set_dpms_self(void) {
    modules[self.idx].self = &self;
    modules[self.idx].init = init;
    modules[self.idx].destroy = destroy;
    set_self_deps(&self);
}

/*
 * Checks through xcb if DPMS is enabled for this xscreen
 */
static void init(void) {    
    connection = xcb_connect(NULL, NULL);

    if (!xcb_connection_has_error(connection)) {
        xcb_dpms_info_cookie_t cookie;
        xcb_dpms_info_reply_t *info;

        cookie = xcb_dpms_info(connection);
        info = xcb_dpms_info_reply(connection, cookie, NULL);

        if (info->state) {
            dpms_enabled = 1;
        }
        // avoid polling this
        init_module(DONT_POLL, self.idx, NULL);
        free(info);
    }
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
    xcb_dpms_info_cookie_t cookie;
    xcb_dpms_info_reply_t *info;
    int ret = -1;

    if (!dpms_enabled) {
        return ret;
    }

    cookie = xcb_dpms_info(connection);
    info = xcb_dpms_info_reply(connection, cookie, NULL);

    if (info) {
        ret = info->power_level;
        free(info);
    }
    return ret;
}

#endif
