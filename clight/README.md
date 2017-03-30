This is clight user service source code.

It currently needs:
* libxcb (xcb.h, xcb/dpms.h)
* libsystemd (systemd/sd-bus.h)
* libconfig (libconfig.h)
* libpopt (popt.h)

A systemd user unit is shipped too. Just run "systemctl --user enable clight.service" and reboot.

Current features:
* very lightweight
* fully valgrind clean
* external signals catching (sigint/sigterm)
* dpms support: it will check current screen powersave level and won't do anything if screen is currently off.
* a single capture mode from cmdline
* gamma support: it will compute sunset and sunrise and will automagically change screen temperature (just like redshift does). STILL WIP.
* conf file interactive creation ([-s|--setup] cmdline option). Conf options are same as cmdline options.

For cmdline options, check clight [-?|--help] [--usage].
