# Clight

[![Build Status](https://travis-ci.org/FedeDP/Clight.svg?branch=master)](https://travis-ci.org/FedeDP/Clight)

Clight is a C user daemon utility that aims to fully manage your display.  
It will automagically change screen backlight level to match ambient brightness, as computed by capturing frames from webcam.  
Moreover, it can manage your screen temperature, just like redshift does.  
Finally, it can dim your screen after a timeout.  

It was heavily inspired by [calise](http://calise.sourceforge.net/wordpress/) in its initial intents.  

## Build deps
* libsystemd >= 221 (systemd/sd-bus.h)
* libpopt (popt.h)
* gsl (gsl/gsl_multifit.h, gsl/gsl_statistics_double.h)
* libconfig (libconfig.h)
* gcc or clang

## Runtime deps:
* shared objects from build libraries
* [clightd](https://github.com/FedeDP/Clightd) >= 1.3-dev

## Optional runtime deps:
* Geoclue2 to automatically retrieve user location (no geoclue and no user position specified will disable GAMMA support)
* Upower to honor timeouts between captures, to use different ambient brightness -> screen backlight matching coefficients, to change dimmer timeout and to change dpms timeouts depending on ac state.

## How to run it
Clight tries to be a 0-conf software; therefore, it installs a desktop file in /etc/xdg/autostart. This way, no matter what's your DE is, if it is xdg-compliant, it will automatically start clight.   User has to do nothing but reboot after installing clight.  
**This is needed to ensure that X is running when clight gets started**, as on systemd there is no proper way of knowing whether X has been started or not.  
Note that desktop file will execute "systemctl --user start clight"; user service will then kick in clightd dependency and will restart itself in case of crash.  

Finally, a desktop file to take a fast screen backlight recalibration ("clight -c"), useful to be binded to a keyboard shortcut, is installed too, and it will show up in your applications menu.  

## Default Enabled Modules
By default Clight enables all its functions, fallbacking to disable them when they are not supported.
This means that these features are all enabled with default values:  
* BRIGHTNESS: to make webcam captures and change screen backlight level to match ambient brightness
* GAMMA: to change screen temperature based on current time of day
* DIMMER: to dim screen after a certain idle time
* DPMS: to switch off screen after a certain idle time

**All these features but BRIGHTNESS can be turned off through cmdline and config file options.**

## Will it eat my battery?
No, it won't. Clight aims to be very power friendly. In fact, thanks to blocking I/O, it uses CPU only when needed.  
Moreover, being very lightweight on resources helps too. Indeed, it will probably help you saving some battery, by setting correct screen backlight for current ambient brightness (thus avoiding wasting battery on max backlight level).

## Current features:
* very lightweight
* fully valgrind and cppcheck clean
* external signals catching (sigint/sigterm)
* systemd user unit and autostart desktop file shipped
* a quick single capture mode (ie: do captures, change screen brightness and leave) is provided, together with a desktop file.
* gamma support: it will compute sunset and sunrise and will automagically change screen temperature (just like redshift does) -> **X only**
* dimmer support: it will dim your screen after specified timeout of user inactivity (ie: no mouse/keyboard) -> **X only**
* geoclue2 support: when launched without [--lat|--lon] parameters, if geoclue2 is available, it will use it to get user location updates. Otherwise gamma support will be disabled.
Location received will be then cached when clight exit. This way, if no internet connection is present (thus geoclue2 cannot give us any location) instead of hanging, clight will load latest available location from cache file.
* different nightly and daily captures timeout (by default 45mins during the night and 10mins during the day; both configurable)
* nice log file, placed in $HOME/.clight.log
* --sunrise/--sunset times user-specified support: gamma nightly temp will be setted at sunset time, daily temp at sunrise time
* more frequent captures inside "events": an event starts by default 30mins before sunrise/sunset and ends 30mins after. Configurable.
* gamma correction tool support can be disabled at runtime (--no-gamma cmdline switch)
* in case of huge brightness drop (> 60%), a new capture will quickly be done (after 15 seconds), to check if this was an accidental event (eg: you changed room and capture happened before you switched on the light) or if brightness has really dropped that much (eg: you switched off the light)
* conf file placed in both /etc/default and $XDG_CONFIG_HOME (fallbacks to $HOME/.config/) support
* only 1 clight instance can be running for same user. You can still invoke a fast capture when an instance is already running, obviously
* sweet inter-modules dependencies management system with "modules"(CAPTURE, GAMMA, LOCATION, etc etc) lazy loading: every module will only be started at the right time, eg: GAMMA module will only be started after a location has been retrieved.
* UPower support, to set longer timeouts between captures while on battery, in order to save some energy.
Moreover, you can set a percentage of maximum settable brightness while on battery.
* You can specify curve points to be used to match ambient brightness to screen backlight from config file. For more info, see [Polynomial fit](https://github.com/FedeDP/Clight#polynomial-fit) section below.
* It will check if current backlight interface is enabled before changing backlight/dimming screen. It will avoid to do any frame capture at all if interface is disabled. It can happen when you use your laptop connected to an external monitor, with internal monitor switched off; thus changing backlight would be useless.
* Gracefully auto-disabling unsupported module (eg: GAMMA on non-X environments)

### Valgrind is run with:

    $ alias valgrind='valgrind --tool=memcheck --leak-check=full --track-origins=yes --show-leak-kinds=all -v'

### Cppcheck is run with:

    $  cppcheck --enable=style --enable=performance --enable=unusedFunction

For cmdline options, check clight [-?|--help] [--usage].  
**Please note that cmdline "--device" and "--backlight" switches require only last part of syspath** (eg: "video0" or "intel_backlight").  
If your backlight interface will completely dim your screen at 0 backlight level, be sure to set in your conf file (or through cmdline option) *lowest_backlight_level* option.

## Config file
A global config file is shipped with clight. It is installed in /etc/default/clight.conf and it is all commented.  
You can customize it or you can copy it in your $XDG_CONFIG_HOME folder (fallbacks to $HOME/.config/) and customize it there.  
Both files are checked when clight starts, in this order: global -> user-local -> cmdline opts.  

## Polynomial fit
Clight makes use of Gnu Scientific Library to compute best-fit around data points as read by clight.conf file.  
Default values on AC are [ 0.0, 0.15, 0.29, 0.45, 0.61, 0.74, 0.81, 0.88, 0.93, 0.97, 1.0 ]; indexes (from 0 to 10 included) are our X axys (ambient brightness), while values are Y axys (screen backlight).  
For example, with default values, at 0.5 ambient brightness (6th value in this array) clight will set a 74% of max backlight level.  
By customizing these values, you can adapt screen backlight curve to meet your needs. Note that values must be between 0.0 and 1.0 obviously.  
Clight supports different curves on different ac states. In fact, by default on BATT points are different from above. See config file for more info.  

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
