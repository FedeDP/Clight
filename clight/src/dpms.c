#include "../inc/dpms.h"
#include <xcb/dpms.h>
#include <stdlib.h>

static xcb_connection_t *connection;
static int dpms_enabled, inited;

/**
 * Checks through xcb if DPMS is enabled for this xscreen
 */
void init_dpms(void) {
    connection = xcb_connect(NULL, NULL);

    if (!xcb_connection_has_error(connection)) {
        xcb_dpms_info_cookie_t cookie;
        xcb_dpms_info_reply_t *info;

        cookie = xcb_dpms_info(connection);
        info = xcb_dpms_info_reply(connection, cookie, NULL);

        if (info->state) {
            dpms_enabled = 1;
        }
        inited = 1;
        free(info);
    }
    INFO("Dpms support started.\n");
}



/**
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

void destroy_dpms(void) {
    if (inited) {
        if (connection) {
            xcb_disconnect(connection);
        }
        INFO("Dpms destroyed.\n");
    }
}
