TODO:  

- [ ] *clight* fix algorithm to set screen brightness based on ambient brightness  
- [ ] *clightd* add support for polkit (is it useful?)
- [x] add actual_brightness method
- [x] setbrightness should check max interface brightness value too
- [x] *clightd* switch to libudev instead of sys/class string?
- [x] *clightd* switch to libudev even for webcam?
- [x] *clight* fix default values for devname and backlight interface
- [ ] *clightd* + *clight* check for ebusy errors
- [ ] *clightd* + *clight* fix PKGBUILD
- [x] *clightd* + *clight* update readmes
- [x] check dbus call ret value in clight
- [x] switch to new dbus interface
- [x] drop journald support
- [x] drop in pkgbuild udev rule, as it is not needed anymore
- [x] make dbus service build even without frameCapture support
- [x] install service file in /usr/share/dbus-1/system-services/
- [x] install bus conf file in /etc/dbus-1/system.d/
- [x] install clightd in /usr/lib/clight/clightd
