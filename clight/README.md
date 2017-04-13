This is clight user service source code.

## It currently needs:
* libxcb (xcb.h, xcb/dpms.h)
* libsystemd >= 221 (systemd/sd-bus.h)
* libpopt (popt.h)

A systemd user unit is shipped too. Just run "systemctl --user enable clight.service" and reboot.

## Current features:
* very lightweight
* fully valgrind and cppcheck clean
* external signals catching (sigint/sigterm)
* dpms support: it will check current screen powersave level and won't do anything if screen is currently off.
* a single capture mode from cmdline
* gamma support: it will compute sunset and sunrise and will automagically change screen temperature (just like redshift does).
* geoclue2 support: when launched without [--lat|--lon] parameters, if geoclue2 is available, it will use it to get user location updates.
* different nightly and daily captures timeout (by default 45mins during the night and 10mins during the day; both configurable.)
* nice log file, placed in $HOME/.clight.log
* --sunrise/--sunset times user-specified support: gamma nightly temp will be setted at sunset time, daily temp at sunrise time.
* more frequent captures inside "events": an event starts 30mins before sunrise/sunset and ends 30mins after sunrise/sunset.
* gamma correction tool support can be disabled at runtime (--no-gamma cmdline switch)
* in case of huge brightness drop (> 60%), a new capture will quickly be done (after 15 seconds), to check if that was an accidental event (eg: you changed room and capture happened before you switched on the light) or if brightness has really dropped that much (eg: you switched off the light).

### Valgrind is run with:

    $ alias valgrind='valgrind --tool=memcheck --leak-check=full --track-origins=yes --show-leak-kinds=all -v'

### Cppcheck is run with:

    $  cppcheck --enable=style --enable=performance --enable=unusedFunction

For cmdline options, check clight [-?|--help] [--usage].  

There is no more option file support through libconfig as cmd line options already expose every setting.  
Moreover, with [systemd unit conf files](https://wiki.archlinux.org/index.php/systemd#Drop-in_files), is really easy to reach same behaviour (ie: systemd unit launched with certain cmdline args).

## Build instructions:
Build and install:

    $ make
    # make install

Uninstall:

    # make uninstall

## Gamma support info
As clightd getgamma function properly supports only 50-steps temperature values (ie if you use "setgamma 6000" and then getgamma, it will return 6000. If you use setgamma 4578, getgamma won't return exactly it; it will return 4566 or something similar.), do not set in your conf not-50-multiple temperatures.
