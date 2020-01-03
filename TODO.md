### 4.1
- [ ] Improve BACKLIGHT inter-operability with external tools: dimmer should avoid using clight current bl as it may be changed from external tools
- [ ] Add a way to store/reload backlight/gamma settings at clight start/stop
- [ ] Improvement: switch off keyboard backlight on dpms /dimmer? Maybe new conf option?
- [ ] Improvement: allow to pause backlight calib on battery (already supported for dpms/dimmer and screen)
- [ ] Improvement: allow users to use different number of captures for each AC state (allow users to set <= 0 captures to disable calib on ac state too)

### 4.X

#### BACKLIGHT multiple-monitors curves
- [ ] Add support for config files to give each monitor its own backlight curves. Something like /etc/clight/clight.conf + /etc/clight/mon.d/$MONITOR_SERIAL.conf (where MONITOR_SERIAL can be found through org.clightd.clightd.Backlight.GetAll)
- [ ] If any conf file is found in /etc/clight/mon.d/, avoid calling SetAll, and just call Set on each serial.
