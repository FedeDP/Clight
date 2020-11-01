## 4.2

### Generic
- [x] Fix keyboard backlight calibration
- [x] Fix wizard mode
- [x] Fix clight desktop file
- [ ] Update wiki pages

### Dpms
- [x] Hook to new Clightd 4.3 Dpms.Changed signal to react to external (through clightd) dpms state changes

### Gamma
- [x] Hook to new Clightd 4.3 Gamma.Changed signal to react to external (through clightd) gamma state changes
- [x] Fix gamma not setting correct temp on resume -> clight is too quick on resume, and X is not yet loaded when it tries to set new screen temperature...
introduce a small timeout of 5s for gamma corrections

### Keyboard
- [x] Fixed bug in set_keyboard_level()

### Clightd 5.0
- [x] Require clightd 5.0
- [x] Port to clightd new API for dpms signal
- [x] Clightd does now support gamma and dpms on wayland too (where corrispective protocols are supported)
- [x] Clightd does now support gamma on tty too!

## 4.3

### Generic
- [ ] Port to libmodule 6.0.0 (?)
- [ ] Bugfix: fix #106 (?)

### BACKLIGHT multiple-monitors curves
- [ ] Add support for config files to give each monitor its own backlight curves. Something like /etc/clight/clight.conf + /etc/clight/mon.d/$MONITOR_SERIAL.conf (where MONITOR_SERIAL can be found through org.clightd.clightd.Backlight.GetAll)
- [ ] If any conf file is found in /etc/clight/mon.d/, avoid calling SetAll, and just call Set on each serial.

### Bus
- [ ] Expose BUS_REQ to make dbus call from custom modules
