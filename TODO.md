### 3.2
- [x] Fix build when LIBSYSTEMD_VERSION is a float (?), #70
- [x] Different timeout for dimmer leaving-dimmed fading transition
- [x] New cmd line option to specify a conf file to be parsed (instead of default $HOME/.conf/clight.conf)

### 3.3

#### GAMMA improvements
- [ ] Add support for different curves for GAMMA (just like we do for BACKLIGHT) for DAY and NIGHT time. (#61)

#### BACKLIGHT multiple-monitors curves
- [ ] Add support for config files to give each monitor its own backlight curves. Something like /etc/clight/clight.conf + /etc/clight/mon.d/$MONITOR_SERIAL.conf (where MONITOR_SERIAL can be found through org.clightd.clightd.Backlight.GetAll)
- [ ] If any conf file is found in /etc/clight/mon.d/, avoid calling SetAll, and just call Set on each serial.

### 3.X/4.0
- [ ] Port to libmodule
