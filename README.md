# Clight

[![Build Status](https://travis-ci.org/FedeDP/Clight.svg?branch=master)](https://travis-ci.org/FedeDP/Clight)

Clight is a C daemon utility to automagically change screen backlight level to match ambient brightness.  
Ambient brightness is computed by capturing frames from webcam.  
Moreover, it can manage your screen temperature, just like redshift does.  

It was heavily inspired by [calise](http://calise.sourceforge.net/wordpress/) in its initial intents.  

## It currently needs:
* libxcb (xcb.h, xcb/dpms.h)
* libsystemd >= 221 (systemd/sd-bus.h)
* libpopt (popt.h)
* libconfig (libconfig.h)

A systemd user unit is shipped too. Just run "systemctl --now --user enable clight.service".  
Systemd unit requires a display manager to be running, as it binds on display-manager.service to knows whether X is started.  
**If you happen to not use a display manager, just add clight to your .xinitrc or in your DE autostart configuration.**  

## Current features:
* very lightweight
* fully valgrind and cppcheck clean
* external signals catching (sigint/sigterm)
* systemd user unit shipped
* dpms support: it will check current screen powersave level and won't do anything if screen is currently off
* a quick single capture mode (ie: do captures, change screen brightness and leave)
* gamma support: it will compute sunset and sunrise and will automagically change screen temperature (just like redshift does)
* geoclue2 support: when launched without [--lat|--lon] parameters, if geoclue2 is available, it will use it to get user location updates
* different nightly and daily captures timeout (by default 45mins during the night and 10mins during the day; both configurable)
* nice log file, placed in $HOME/.clight.log
* --sunrise/--sunset times user-specified support: gamma nightly temp will be setted at sunset time, daily temp at sunrise time
* more frequent captures inside "events": an event starts 30mins before sunrise/sunset and ends 30mins after
* gamma correction tool support can be disabled at runtime (--no-gamma cmdline switch)
* in case of huge brightness drop (> 60%), a new capture will quickly be done (after 15 seconds), to check if this was an accidental event (eg: you changed room and capture happened before you switched on the light) or if brightness has really dropped that much (eg: you switched off the light)
* conf file placed in both /etc/default and $XDG_CONFIG_HOME (fallbacks to $HOME/.config/) support
* only 1 clight instance can be running for same user. You can still invoke a fast capture when an instance is already running, obviously

### Valgrind is run with:

    $ alias valgrind='valgrind --tool=memcheck --leak-check=full --track-origins=yes --show-leak-kinds=all -v'

### Cppcheck is run with:

    $  cppcheck --enable=style --enable=performance --enable=unusedFunction

For cmdline options, check clight [-?|--help] [--usage].  

## Config file
A global config file is shipped with clight. It is installed in /etc/default/clight.conf and it is all commented.  
You can customize it or you can copy it in your $XDG_CONFIG_HOME folder (fallbacks to $HOME/.config/) and customize it there.  
Both files are checked when clight starts, in this order: global -> user-local -> cmdline opts.  

## Gamma support info
As [clightd](https://github.com/FedeDP/Clightd#devel-info) getgamma function properly supports only 50-steps temperature values (ie if you use "setgamma 6000" and then getgamma, it will return 6000. If you use setgamma 4578, getgamma won't return exactly it; it will return 4566 or something similar.), do not set in your conf not-50-multiple temperatures.  
Moreover, since there is still no standard way to deal with gamma correction on wayland, it is only supported on X11.  
If you run clight from wayland or from a tty, gamma support will be automatically disabled.  
Finally, gamma support needs "xhost +" for your session, as otherwise bus interface running from root cannot access your X screen.  
Systemd user service already does this for you. If you are using an autostart script, remember to call "xhost +" before starting clight.

## Other info
You can only run one clight instance per-user: if a clight instance is running, you cannot start another full clight instance.  
Obviously you can still invoke "clight -c" from a terminal/shortcut to make a fast capture/screen brightness calibration.  
This is achieved through a clight.lock file placed in current user home.

## Build instructions:
Build and install:

    $ make
    # make install

Uninstall:

    # make uninstall

## Arch AUR packages
Clight is available on AUR: https://aur.archlinux.org/packages/clight-git/ .

## Deb packages
Deb package for amd64 architecture is provided. You can find it inside [Debian](https://github.com/FedeDP/Clight/tree/master/Debian) folder.  
Moreover, you can very easily build your own packages, if aforementioned package happens to be outdated.  
You only need to issue:

    $ make deb

A deb file will be created in [Debian](https://github.com/FedeDP/Clight/tree/master/Debian) folder.  
Please note that while i am using Debian testing at my job, i am developing clight from archlinux.  
Thus, you can encounter some packaging issues. Please, report back.  

## License
This software is distributed with GPL license, see [COPYING](https://github.com/FedeDP/Clight/blob/master/COPYING) file for more informations.
