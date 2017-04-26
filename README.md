# Clight

[![Build Status](https://travis-ci.org/FedeDP/Clight.svg?branch=master)](https://travis-ci.org/FedeDP/Clight)

Clight is a C daemon utility to automagically change screen backlight level to match ambient brightness.  
Ambient brightness is computed by capturing frames from webcam.  
Moreover, it can manage your screen temperature, just like redshift does.  

It was heavily inspired by [calise](http://calise.sourceforge.net/wordpress/) in its initial intents.  

## Build deps
* libxcb (xcb.h, xcb/dpms.h)
* libsystemd >= 221 (systemd/sd-bus.h)
* libpopt (popt.h)
* libconfig (libconfig.h)

## Runtime deps:
* shared objects from build libraries
* [clightd](https://github.com/FedeDP/Clightd)

## How to run it
Even if a systemd user unit is shipped, proper way to start clight is to put into your DE session autostart manager/X autostart script:

    $ systemctl --user start clight.service
    
**This is needed to assure that X is running when clight gets started**.  
On systemd there is no proper way of knowing whether X has been started or not.  
Simply enabling the unit could cause clight to auto-disabling gamma support if it cannot find any running X server.  
Note that is better to use that command to start the user service instead of just using "clight", because the service kicks in clightd dependency.  
Moreover, it will make clight restart if any issue happens.

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
* sweet inter-modules dependencies management system with "modules"(CAPTURE, GAMMA, LOCATION, etc etc) lazy loading: every module will only be started at the right time, eg: GAMMA module will only be started after a location has been retrieved.

### Valgrind is run with:

    $ alias valgrind='valgrind --tool=memcheck --leak-check=full --track-origins=yes --show-leak-kinds=all -v'

### Cppcheck is run with:

    $  cppcheck --enable=style --enable=performance --enable=unusedFunction

For cmdline options, check clight [-?|--help] [--usage].  
**Please note that cmdline "--device" and "--backlight" switches require only last part of syspath** (eg: "video0" or "intel_backlight")
If your backlight interface will completely dim your screen at 0 backlight level, be sure to set in your conf file (or through cmdline option) the "lowest_backlight_level" option.

## Config file
A global config file is shipped with clight. It is installed in /etc/default/clight.conf and it is all commented.  
You can customize it or you can copy it in your $XDG_CONFIG_HOME folder (fallbacks to $HOME/.config/) and customize it there.  
Both files are checked when clight starts, in this order: global -> user-local -> cmdline opts.  

## Gamma support info
*Gamma support is only available on X. Sadly on wayland there is still no standard way to achieve gamma correction. Let's way with fingers crossed.*  
Consequently, on not X environments, gamma correction tool gets autodisabled.  

As [clightd](https://github.com/FedeDP/Clightd#devel-info) getgamma function properly supports only 50-steps temperature values (ie if you use "setgamma 6000" and then getgamma, it will return 6000. If you use setgamma 4578, getgamma won't return exactly it; it will return 4566 or something similar.), do not set in your conf not-50-multiple temperatures.  
Moreover, since there is still no standard way to deal with gamma correction on wayland, it is only supported on X11.  
If you run clight from wayland or from a tty, gamma support will be automatically disabled.  

## Other info
You can only run one clight instance per-user: if a clight instance is running, you cannot start another full clight instance.  
Obviously you can still invoke "clight -c" from a terminal/shortcut to make a fast capture/screen brightness calibration.  
This is achieved through a clight.lock file placed in current user home.

Every functionality in clight is achieved through a "module". An inter-modules dependencies system has been created ad-hoc to ease development of such modules.  
This way, it does not matter modules' init calls sorting; moreover, each module can be easily disabled if not needed (eg: when --no-gamma option is passed.)

## Build instructions:
Build and install:

    $ make
    # make install

Uninstall:

    # make uninstall

## Arch AUR packages
Clight is available on AUR: https://aur.archlinux.org/packages/clight-git/ .

## Deb packages
Deb package for amd64 architecture is provided for each [release](https://github.com/FedeDP/Clight/releases).  
Moreover, you can very easily build your own packages, if you wish to test latest Clight code.  
You only need to issue:

    $ make deb

A deb file will be created in "Debian" folder, inside Clight root.  
Please note that while i am using Debian testing at my job, i am developing clightd from archlinux.  
Thus, you can encounter some packaging issues. Please, report back.  

## License
This software is distributed with GPL license, see [COPYING](https://github.com/FedeDP/Clight/blob/master/COPYING) file for more informations.
