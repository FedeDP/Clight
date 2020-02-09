## 4.1

### Backlight
- [x] Improvement: switch off keyboard backlight on dpms/dimmer? Maybe new conf option?
- [x] Improvement: allow to pause backlight calib on battery (already supported for dpms/dimmer and screen) by setting timeout <= 0
- [x] Improvement: allow users to use different number of captures for each AC state (?)
- [ ] Improvement: switch off keyboard backlight above certain ambient brightness threshold (#112)
- [x] Bugfix: screen-emitted-brightness compensation should not directly change state.ambient_br
- [x] Bugfix: properly clamp compensated_br between 0 and 1 before setting new backlight
- [x] Bugfix: Properly set 0.0 backlight level if shutter_threshold is 0.0 (>= instead of >)
- [x] Bugfix: set_new_backlight should take current ambient_br * conf.num_points (that is no more fixed to 10!)
- [x] Drop inhibit_autocalib (to be demanded to inhibit_bl custom module)

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
- [ ] Improvement: improve bus::call() function
- [ ] Improvement: actually read sensor_devname reported by clightd Capture call; this way users can check from DBus API which sensor has been used
- [ ] Bugfix: fix #106

## 4.2

### Generic
- [ ] Improve inter-operability with external tools: dimmer should avoid using clight current bl as it can be changed by external tools
- [ ] Add a way to store/reload backlight/gamma settings at clight start/stop

## 4.X

### BACKLIGHT multiple-monitors curves
- [ ] Add support for config files to give each monitor its own backlight curves. Something like /etc/clight/clight.conf + /etc/clight/mon.d/$MONITOR_SERIAL.conf (where MONITOR_SERIAL can be found through org.clightd.clightd.Backlight.GetAll)
- [ ] If any conf file is found in /etc/clight/mon.d/, avoid calling SetAll, and just call Set on each serial.
