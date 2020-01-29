### 4.1

#### BACKLIGHT
- [ ] Improvement: switch off keyboard backlight on dpms /dimmer? Maybe new conf option?
- [ ] Improvement: allow to pause backlight calib on battery (already supported for dpms/dimmer and screen) by setting timeout <= 0
- [ ] Improvement: allow users to use different number of captures for each AC state
- [ ] Improvement: switch off keyboard backlight above certain ambient brightness threshold (#112)
- [x] Improvement: rework log_conf() function to print configs MODULE based, just like conf file

- [x] Bugfix: screen-emitted-brightness compensation should not directly change state.ambient_br
- [x] Bugfix: properly clamp compensated_br between 0 and 1 before setting new backlight
- [x] Bugfix: Properly set 0.0 backlight level if shutter_threshold is 0.0 (>= instead of >)
- [x] Bugfix: set_new_backlight should take current ambient_br * conf.num_points (that is no more fixed to 10!)

### 4.2

#### Generic
- [ ] Improve inter-operability with external tools: dimmer should avoid using clight current bl as it can be changed by external tools
- [ ] Add a way to store/reload backlight/gamma settings at clight start/stop

### 4.X

#### BACKLIGHT multiple-monitors curves
- [ ] Add support for config files to give each monitor its own backlight curves. Something like /etc/clight/clight.conf + /etc/clight/mon.d/$MONITOR_SERIAL.conf (where MONITOR_SERIAL can be found through org.clightd.clightd.Backlight.GetAll)
- [ ] If any conf file is found in /etc/clight/mon.d/, avoid calling SetAll, and just call Set on each serial.
