# Clight

[![Build Status](https://travis-ci.org/FedeDP/Clight.svg?branch=master)](https://travis-ci.org/FedeDP/Clight)
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/3dabfb28d8db4f25955ecb82c30447dc)](https://www.codacy.com/app/FedeDP/Clight?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=FedeDP/Clight&amp;utm_campaign=Badge_Grade)

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
* [clightd](https://github.com/FedeDP/Clightd) >= 2.0

## Optional runtime deps:
* Geoclue2 to automatically retrieve user location (no geoclue and no user position specified will disable GAMMA support)
* Upower to honor timeouts between captures, to use different ambient brightness -> screen backlight matching coefficients, to change dimmer timeout and to change dpms timeouts depending on ac state.

## How to run it
Clight tries to be a 0-conf software; therefore, you only have to enable the clight service:

    $ systemctl --user enable clight.timer

Note that timer is needed to ensure that any needed bus interface is completely started; it will delay clight startup of 5s. User service will then kick in clightd dependency.  

Finally, a desktop file to take a fast screen backlight recalibration, useful to be binded to a keyboard shortcut, is installed too, and it will show up in your applications menu.  

## Geoclue issues
If you have got any issue with geoclue, please try to append following lines to /etc/geoclue/geoclue.conf:

```
[clightc]
allowed=true
system=false
users=
```

If issues persist, please fill a bug report.

## Default Enabled Modules
By default Clight enables all its functions, fallbacking to disable them when they are not supported.
This means that these features are all enabled with default values:  
* BRIGHTNESS: to make webcam captures and change screen backlight level to match ambient brightness
* GAMMA: to change screen temperature based on current time of day (X-only feature)
* DIMMER: to dim screen after a certain idle time (X-only feature)
* DPMS: to switch off screen after a certain idle time (X-only feature)

**All these features can be turned off through cmdline and config file options.**

## Will it eat my battery?
No, it won't. Clight aims to be very power friendly. In fact, thanks to blocking I/O, it uses CPU only when needed.  
Moreover, being very lightweight on resources helps too. Indeed, it will probably help you saving some battery, by setting correct screen backlight for current ambient brightness (thus avoiding wasting battery on max backlight level).  
Finally, remember that webcam is not always on; it is used only when needed with timeouts between clusters of captures thoroughly thought and configurable (with longer timeouts while on battery).

## Current features:
* very lightweight
* fully valgrind and cppcheck clean
* external signals catching (sigint/sigterm)
* systemd user unit shipped
* gamma support: it will compute sunset and sunrise and will automagically change screen temperature (just like redshift does) -> **X only**
* dimmer support: it will dim your screen after specified timeout of user inactivity (ie: no mouse/keyboard) -> **X only**
* geoclue2 support: when launched without [--lat|--lon] parameters, if geoclue2 is available, it will use it to get user location updates. Otherwise gamma support will be disabled.
Location received will be then cached when clight exit. This way, if no internet connection is present (thus geoclue2 cannot give us any location), clight will load latest available location from cache file. If no cached location is present, gamma will be disabled.
* nice log file, placed in $HOME/.clight.log
* --sunrise/--sunset times user-specified support: gamma nightly temp will be setted at sunset time, daily temp at sunrise time
* more frequent captures inside "events": an event starts by default 30mins before sunrise/sunset and ends 30mins after. Configurable.
* conf file placed in both /etc/default and $XDG_CONFIG_HOME (fallbacks to $HOME/.config/) support
* Lots of configurations available
* only 1 clight instance can be running for each user. You can still invoke a fast capture when an instance is already running, obviously
* sweet inter-modules dependencies management system with "modules"(BRIGHTNESS, GAMMA, LOCATION, etc etc) lazy loading: every module will only be started at the right time, eg: GAMMA module will only be started after a location has been retrieved. Moreover, modules gets gracefully auto-disabled where unsupported (eg: GAMMA on non-X environments)
* UPower support, to set longer timeouts between captures while on battery, in order to save some energy.
* You can specify curve points to be used to match ambient brightness to screen backlight from config file. For more info, see [Polynomial fit](https://github.com/FedeDP/Clight#polynomial-fit) section below.
* DPMS support: it will set desired dpms timeouts for AC/batt states.
* Dpms and dimmer can be disabled while on AC, just set dimmer timeout/any dpms timeout for given AC state <= 0.
* Clight supports org.freedesktop.PowerManagement.Inhibit interface. Thus, when for example watching a youtube video from chromium, dimmer module won't dim your screen.
* Supports both internal laptop monitor and external monitors (thus desktop PCs too), thanks to [ddcutil](https://github.com/rockowitz/ddcutil).
* Autodisable brightness module on PCs without webcam
* Check clightd version on clight startup

### Valgrind is run with:

    $ alias valgrind='valgrind --tool=memcheck --leak-check=full --track-origins=yes --show-leak-kinds=all -v'

### Cppcheck is run with:

    $  cppcheck --enable=style --enable=performance --enable=unusedFunction

For cmdline options, check clight [-?|--help] [--usage].  

## Config file
A global config file is shipped with clight. It is installed in /etc/default/clight.conf and it is full of comments.  
You can customize it or you can copy it in your $XDG_CONFIG_HOME folder (fallbacks to $HOME/.config/) and customize it there.  
Both files are checked when clight starts, in this order: global -> user-local -> cmdline opts.  

## Polynomial fit
Clight makes use of Gnu Scientific Library to compute best-fit around data points as read by clight.conf file.  
Default values on AC are [ 0.0, 0.15, 0.29, 0.45, 0.61, 0.74, 0.81, 0.88, 0.93, 0.97, 1.0 ]; indexes (from 0 to 10 included) are our X axys (ambient brightness), while values are Y axys (screen backlight).  
For example, with default values, at 0.5 ambient brightness (6th value in this array) clight will set a 74% of max backlight level.  
By customizing these values, you can adapt screen backlight curve to meet your needs. Note that values must be between 0.0 and 1.0 obviously.  
Clight supports different curves on different ac states. In fact, by default on BATT points are different from above. See config file for more info.  

## Gamma support info
*Gamma support is only available on X. Sadly on wayland there is still no standard way to achieve gamma correction. Let's wait with fingers crossed.*  
Consequently, on not X environments, gamma correction tool gets autodisabled.  

As [clightd](https://github.com/FedeDP/Clightd#devel-info) getgamma function properly supports only 50-steps temperature values (ie if you use "setgamma 6000" and then getgamma, it will return 6000. If you use setgamma 4578, getgamma won't return exactly it; it will return 4566 or something similar.), do not set in your conf not-50-multiple temperatures.  
Moreover, since there is still no standard way to deal with gamma correction on wayland, it is only supported on X11: if you run clight from wayland or from a tty, gamma support will be automatically disabled.

## Other info
You can only run one clight instance per-user: if a clight instance is running, you cannot start another full clight instance.  
This is achieved through a clight.lock file placed in current user home.

Every functionality in clight is achieved through a "module". An inter-modules dependencies system has been created ad-hoc to ease development of such modules.  
This way, it does not matter modules' init calls sorting; moreover, each module can be easily disabled if not needed/desired.

## Build instructions:
Build and install:

    $ make
    # make install

Uninstall:

    # make uninstall

## Arch AUR packages
Clight is available on AUR: https://aur.archlinux.org/packages/clight-git/ .

## License
This software is distributed with GPL license, see [COPYING](https://github.com/FedeDP/Clight/blob/master/COPYING) file for more informations.
