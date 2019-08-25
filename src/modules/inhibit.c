#include <bus.h>

static void publish_inhibit(int old, int new, inhibit_upd *up);

static inhibit_upd inh_msg = { INHIBIT_UPD };

MODULE("INHIBIT");

static void init(void) {
    M_SUB(INHIBIT_REQ);
}

static bool check(void) {
    return true;
}

static bool evaluate() {
    return true;
}

static void receive(const msg_t *const msg, const void* userdata) {
    if (msg->is_pubsub && msg->ps_msg->type == USER) {
        MSG_TYPE();
        switch (type) {
        case INHIBIT_REQ: {
            inhibit_upd *up = (inhibit_upd *)msg->ps_msg->message;
            state.inhibited = up->new;
            INFO("ScreenSaver inhibition %s.\n", state.inhibited ? "enabled" : "disabled");
            publish_inhibit(up->old, up->new, &inh_msg);
            }
            break;
        default:
            break;
        }
    }
}

static void destroy(void) {

}

static void publish_inhibit(int old, int new, inhibit_upd *up) {
    up->old = old;
    up->new = new;
    M_PUB(up);
}
