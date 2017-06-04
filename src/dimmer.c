#include "../inc/dimmer.h"
#include <sys/inotify.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/scrnsaver.h>

static void init(void);
static int check(void);
static void destroy(void);
static void dimmer_cb(void);
static int get_idle_time(void);
static void check_afk_time(void);

static int inot_wd, inot_fd, timer_fd, idle_time;
static struct self_t self = {
    .name = "Dimmer",
    .idx = DIMMER,
};

void set_dimmer_self(void) {
    modules[self.idx].self = &self;
    modules[self.idx].init = init;
    modules[self.idx].check = check;
    modules[self.idx].destroy = destroy;
    set_self_deps(&self);
}

static void init(void) {
    inot_fd = inotify_init();
    if (inot_fd != -1) {
        timer_fd = start_timer(CLOCK_MONOTONIC, 30, 0);
        init_module(timer_fd, self.idx, dimmer_cb);
    }
}

// TODO: check we're on X?
static int check(void) {
    return 0; /* Skeleton function needed for modules interface */
}

static void destroy(void) {
    if (state.is_dimmed) {
        inotify_rm_watch(main_p[self.idx].fd, inot_wd);
        if (timer_fd > 0) {
            close(timer_fd);
        }
    } else if (inot_fd > 0) {
        close(inot_fd);
    }
}

static void dimmer_cb(void) {
    uint64_t t;
    int r;
    // FIXME: read all inotify events here...
    do {
        r = read(main_p[self.idx].fd, &t, sizeof(uint64_t));
    } while (r > 0);
    
    if (!state.is_dimmed) {
        check_afk_time();
        if (state.is_dimmed) {
            printf("Dimmed!\n");
            // TODO: lower backlight
            inot_wd = inotify_add_watch(inot_fd, "/dev/input/", IN_ACCESS);
            // TODO: check if inot_wd == -1
            main_p[self.idx].fd = inot_fd;
        } else if (idle_time != -1) {
            set_timeout(30 - idle_time, 0, main_p[self.idx].fd, 0);
            printf("timeout of %d\n", 30 - idle_time);
        }
    } else {
        printf("out of dimmed state!\n");
        state.is_dimmed = 0;
        inotify_rm_watch(inot_fd, inot_wd);
        main_p[self.idx].fd = timer_fd;
        set_timeout(30, 0, main_p[self.idx].fd, 0);
        // TODO: restore correct backlight level
    }
}

static int get_idle_time(void) {
    time_t idle_time;
    static XScreenSaverInfo *mit_info;
    Display *display;
    int screen;
    mit_info = XScreenSaverAllocInfo();
    if (!(display=XOpenDisplay(NULL))) { 
        return -1; 
    }
    screen = DefaultScreen(display);
    XScreenSaverQueryInfo(display, RootWindow(display,screen), mit_info);
    idle_time = (mit_info->idle) / 1000;
    XFree(mit_info);
    XCloseDisplay(display); 
    return idle_time;
}

static void check_afk_time(void) {
    idle_time = get_idle_time();
    if (idle_time != -1) {
        /* -1 as it seems we receive events circa 1s before */
        state.is_dimmed = idle_time >= (30 - 1);
    }
}

