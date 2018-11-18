## 3.0

### New Clightd
- [x] BRIGHTNESS module will add a match on clightd SensorChanged signal and will react to it appropriately (leave pause state when sensor becomes available, enter paused state when sensor is not available)
- [x] BRIGHTNESS module will start paused if no sensors are available; DIMMER module will then stay not-inited until a sensor becomes available (because until first capture is done, we do not knok current_br_pct)
- [x] Add support for new Clightd interface
- [x] Update to new CaptureX, IsXAvailable clightd api
- [x] Add back support for new CaptureSensor interface ("sad" return type instead of "sd")
- [x] CaptureSensor already return values between 0 and 1.0. Avoid "/ 255" while computing mean value.
- [x] Update to new Idle interface
- [x] Dimmer -> properly fix for dimmer_timeouts <= 0

### Fixes
- [x] Do not require both --sunrise and --sunset options!
- [x] Do not leave clight if --sunrise or --sunset are wrong, start gamma and location instead
- [x] Dimmer should set backlight_level to 100% if brightness module is disabled
- [ ] At start clight hangs 20s on first sd_bus_call, casued by geoclue2 GetClient:  
(D)[10:16:55]{./modules.c:148}  Trying to start LOCATION module as its BUS dependency was loaded...  
(D)[10:17:15]{./utils/timer.c:41}       Set timeout of 3s 0ns on fd 7.  
https://github.com/jonls/redshift/issues/645 AND https://gitlab.freedesktop.org/geoclue/geoclue/issues/84  
THIS NEEDS TO BE SOLVED BY GEOCLUE. /usr/lib/geoclue-2.0/demos/where-am-i takes insane amount of time too (20s)  

### Improvements
- [x] Add a secondary option to desktop file to Inhibit dimmer (https://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#extra-actions)
- [x] in bus.c functions, try to propagate caller FUNCNAME (with a macro?) to check_err() and log it
- [x] Drop systemd timer and ask user to add "systemctl --user start clight" to their DE/WM/WaylandCompositor starting script and add back a desktop file for xdg autostart?
- [x] Add new Wiki page on how to start clight
- [x] Drop clight systemd service
- [x] Rename BRIGHTNESS module to BACKLIGHT
- [x] Store and expose latest mean ambient brightness as captured by clightd
- [ ] Port CI to sr.ht
- [ ] Document each module's dependencies

### Bus API
- [x] add bus interface methods to change timeouts (dimmer, dpms and brightness modules)
- [x] Add a bus interface method to change video interface and backlight sysname while running
- [x] Expose all conf structure through interface in org/clight/clight/Conf
- [x] Add a "StoreConf" method that dumps current config to user conf
- [x] Remove struct location loc from conf, and put it in state
- [x] FIX: properly account for state.time == EVENT in SetGamma bus call
- [x] FILL_MATCH_DATA() macro to forcefully use state.userdata instead of relying on casting void *userdata
- [x] use FILL_MATCH_DATA for exposed configurations too
- [x] Remove ApplyGamma and automatically set new temperature when DailyTemp/NightTemp are changed
- [x] Rename "StoreConf" to only "Store" as it is already under /org/clight/clight/Conf path

### Passive mode
- [x] Functional module can also be started in "passive" mode, ie: they are started but paused, and will remain paused. You can only trigger them through bus API.

### TEST
- [x] Test SensorChanged signal

### RELEASE

- [x] Update API doc
- [ ] Improve MINIMUM_CLIGHTD_VERSION_MAJ to 3

### 3.1

- [ ] Add loglevel conf instead of only --verbose option?
- [ ] Write a plasma5-applet? Eg: start from plasma-redshift applet

## Ideas
- [ ] Use the Time PropertiesChanged signal to change KDE/GNOME theme at sunset/sunrise 
- [ ] GNOME: http://www.fandigital.com/2012/06/change-theme-command-line-gnome.html / https://askubuntu.com/questions/546402/how-to-change-gnome-shell-theme-in-ubuntu-14-10)
- [ ] KDE: https://userbase.kde.org/KDE_Connect/Tutorials/Useful_commands#Change_look_and_feel
- [ ] Add a calibration mode?
