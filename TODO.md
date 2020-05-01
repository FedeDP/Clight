## 4.1

### Backlight
- [x] Improvement: switch off keyboard backlight on dpms/dimmer? Maybe new conf option?
- [x] Improvement: allow to pause backlight calib on battery (already supported for dpms/dimmer and screen) by setting timeout <= 0
- [x] Improvement: allow users to use different number of captures for each AC state (?)
- [x] Bugfix: screen-emitted-brightness compensation should not directly change state.ambient_br
- [x] Bugfix: properly clamp compensated_br between 0 and 1 before setting new backlight
- [x] Bugfix: Properly set 0.0 backlight level if shutter_threshold is 0.0 (>= instead of >)
- [x] Bugfix: set_new_backlight should take current ambient_br * conf.num_points (that is no more fixed to 10!)
- [x] Drop inhibit_autocalib (to be demanded to inhibit_bl custom module)
- [x] Bugfix: correctly manage pubsub messages when in paused state
- [x] Properly support new Clightd API that only returns number of captures successfully retrieved

### Keyboard
- [x] Split keyboard settings from backlight
- [x] Improvement: switch off keyboard backlight above certain ambient brightness threshold (#112)
- [x] Extract new Keyboard module

### DPMS
- [x] Make sure to actually restore DISPLAY_ON state from DPMS module if DIMMER is disabled. Fix #120

### Inhibition
- [x] Fix support for multiple inhibition sources; eg: both "docked" and "ScreenSaver" are inhibiting clight. When ScreenSaver drops its inhibition, state is set back to not inhibited.
- [x] Fix "Failed to parse parameters: No such device or address" message
- [x] Log both app name and reason during inhibit; log only app name during Uninhibit
- [x] Add a force option to inhibit_upd msg requests, used by clight Inhibit "false" dbus interface, to forcefully remove any inhibiton

### New Conf file layout
- [x] Use libconfig "groups" instead of new conf files eg: { Camera = ac_backlight_regression = [ ... ]; ... }
- [x] Split sensor-related settings from conf_t
- [x] Move number of captures to sensor specific settings?
- [x] Update log_conf
- [x] Split check_conf() in check_bl_conf... etc etc
- [x] Fix INTERFACE: expose Conf/Backlight, conf/Gamma etc etc??
- [x] Rename interface conf names as they are now in different paths thus we can duplicate them
- [x] Rename conf_t fields (they can now be repeated inside each module conf struct, eg: no_gamma/no_backlight -> bool disabled)

### UPower
- [x] Improvement: only start Upower module on laptops -> "LidIsPresent" property?
- [x] Improvement: add "Docked" state management
- [x] Docked signal internally will send an inhibit request and thus will be managed as inhibition, pausing dimmer and dpms and eventually backlight autocalib
- [x] Improvement: add a new "LidClosed" signal 
- [x] Use LidClosed signal to eventually pause calibration -> eg: inhibit_on_lid_closed = true BACKLIGHT conf. Only if !docked obviously. 
- [x] Expose LidClosed property through dbus interface

### Generic
- [x] Improvement: rework log_conf() function to print configs MODULE based, just like conf file
- [x] Improvement: add a "reason" field to inhibit_requests type
- [x] Improvement: improve bus::call() function
- [x] Improvement: actually read sensor_devname reported by clightd Capture call; this way users can check from log which sensor has been used
- [ ] Bugfix: fix #106 (?)
- [x] Rework conf_t struct
- [x] Use DEBUG loglevel instead of WARN in validations. Not that useful in production
- [x] Check with valgrind
- [x] Double check to not use any variable from disabled modules
- [x] Avoid logging disabled modules conf as it is not checked
- [x] Avoid creating disabled modules conf interafce
- [x] Kill clight on bus disconnection (ENOTCONN/ECONNRESET errnos)

## 4.2

### Generic
- [ ] Improve inter-operability with external tools: dimmer should avoid using clight current bl as it can be changed by external tools
- [ ] Add a way to store/reload backlight/gamma settings at clight start/stop

### BACKLIGHT multiple-monitors curves
- [ ] Add support for config files to give each monitor its own backlight curves. Something like /etc/clight/clight.conf + /etc/clight/mon.d/$MONITOR_SERIAL.conf (where MONITOR_SERIAL can be found through org.clightd.clightd.Backlight.GetAll)
- [ ] If any conf file is found in /etc/clight/mon.d/, avoid calling SetAll, and just call Set on each serial.
