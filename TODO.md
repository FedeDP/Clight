## 2.5
- [x] Fix geoclue2 issues
- [x] Install license file in /usr/share/licenses/clight/
- [x] Drop support for /etc/xdg/autostart, and tell user to issue a "systemctl --user enable clight.timer" instead. This way even not-xdg-compliant DE will work out of the box
- [x] add a small bus interface to query clight status/set new timeouts/make new fast capture ("-c" switch to follow this new api)
- [x] Clight single capture log should be logged to same log as main clight (obviously as it will be now redirected to new api (that will just reset brightness timeout to 1ns))
- [x] Clightd module should depend on bus
- [x] Automatically set module self at startup
- [x] Drop unused code
- [x] Require c99 as no more c11 is used
- [x] Better code organization and repo architecture

- [x] Improve bus interface with more methods (setgamma + inhibit (presentation mode))
- [x] rename INITED state to RUNNING state
- [x] Dropped state.fast_recapture leftover references
- [x] Drop lock files: when INTERFACE module requests a bus name, request will fail if another daemon is already registered
- [x] Add completion script for bash
- [x] Dropped timeouts setting from cmdline options, way too chaotic
- [x] Add a bus signal "TimeChanged" on sunrise/sunset that returns an enum time { SUNRISE, SUNSET }
- [x] Add querySunrise/querySunset to interface
- [x] Use log file as lock; avoid leaving clight if interface name can not be requested
- [x] Pause BRIGHTNESS module in dimmed state (and remove if (!state.is_dimmed) check in brightness.c)
- [x] BRIGHTNESS module will add a match on Dimmed and Time signals
- [x] Start Inhibit anyway if initial polling fails: interface may be added later and we are allowed to add a match on non-existent interface
- [x] Use sd_bus_match_signal instead of sd_bus_add_match
- [x] Expose state.time and state.is_dimmed too
- [x] Use sd_bus_emit_properties_changed instead of emit_signal on exposed properties (drop 2 signals)

- [x] Fix clight_callback and upower_callback in BRIGHTNESS
- [x] Drop clight timer
- [X] Polynomial fit to take an array (without forcing conf.regression_points)
- [x] Avoid overriding conf.regression_points with user passed data.
- [x] Properly process USERBUS too before start looping on events
- [x] Add a bus service file
- [x] Improve bus match callbacks handling

- [x] Improve bus callback managing and modules bus callbacks -> eg: BRIGHTNESS module has a very chaotic implementation for its bus callbacks. Simplify it.

- [x] Force PM inhibition ON when forced by bus interface

### TEST
- [x] BRIGHTNESS MODULE callback on clight state changes

### Doc
- [x] Add API reference for bus interface
- [x] Add gh wiki pages (?) for "How to build", "features", "Geoclue2 issues", "Bus API"...
- [x] Remove everything from readme

## 3.0
- [ ] BRIGHTNESS module will add a match on clightd WebcamChanged signal if clightd >= 2.4 is found, and will react to it appropriately (avoid requiring clightd >= 2.4 though)
- [ ] Do not require both --sunrise and --sunset options!
- [ ] Do not leave clight if --sunrise or --sunset are wrong, just disable gamma
- [ ] Add support for new Clightd ALS interface
- [ ] Add a module pause function that stores current timeout and pauses module + a resume fn to restore it (use it for backlight when going dimmed, and wherever needed)
- [ ] Xorg module to wait on x11 socket before starting (/tmp/.X11-unix/X0 i guess) or something like that (inotify watcher on /tmp folder waiting for .X11-unix folder to appear?) and drop systemd timer unit

## Ideas
- [ ] Use the Time PropertiesChanged signal to change KDE/GNOME theme at sunset/sunrise 
- [ ] GNOME: http://www.fandigital.com/2012/06/change-theme-command-line-gnome.html / https://askubuntu.com/questions/546402/how-to-change-gnome-shell-theme-in-ubuntu-14-10)
- [ ] KDE: https://userbase.kde.org/KDE_Connect/Tutorials/Useful_commands#Change_look_and_feel
