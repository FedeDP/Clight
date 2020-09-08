## 4.2

### Generic
- [x] Require clightd 4.3
- [x] Fix keyboard backlight calibration
- [ ] Bugfix: fix #106 (?)
- [ ] Port to libmodule 6.0.0 (?)

### Dpms
- [x] Hook to new Clightd 4.3 Dpms.Changed signal to react to external (through clightd) dpms state changes

### Gamma
- [x] Hook to new Clightd 4.3 Gamma.Changed signal to react to external (through clightd) gamma state changes

### Keyboard
- [x] Fixed bug in set_keyboard_level()

### Bus
- [ ] Expose BUS_REQ to make dbus call from custom modules

### BACKLIGHT multiple-monitors curves
- [ ] Add support for config files to give each monitor its own backlight curves. Something like /etc/clight/clight.conf + /etc/clight/mon.d/$MONITOR_SERIAL.conf (where MONITOR_SERIAL can be found through org.clightd.clightd.Backlight.GetAll)
- [ ] If any conf file is found in /etc/clight/mon.d/, avoid calling SetAll, and just call Set on each serial.
