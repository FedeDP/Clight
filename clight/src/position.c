#include "../inc/position.h"

/* Skeleton for our future interface that will use geoclue */
int init_position(void) {
    // fd used to notify main poll that a new position has been received
    int position_fd = -1;

    if (conf.lat != 0 && conf.lon != 0) {
        // signal main_poll on this eventf that position is already available
        // as it is setted through a cmdline option
        position_fd = eventfd(0, O_NONBLOCK); // new in 2.6.27
        if (position_fd == -1) {
            fprintf(stderr, "%s\n", strerror(errno));
            state.quit = 1;
        } else {
            uint64_t value = 1;
            int r = write(position_fd, &value, sizeof (value));
            if (r == -1) {
                fprintf(stderr, "%s\n", strerror(errno));
                state.quit = 1;
            }
        }
    } /* else {
        // let main poll listen on new position events coming from geoclue
        geoclue_support();

        position_fd = sd_bus_get_fd(bus); // to receive geoclue notifications
    } */
    return position_fd;
}
