### 3.2
- [x] Fix build when LIBSYSTEMD_VERSION is a float (?), #70
- [x] Different timeout for dimmer leaving-dimmed fading transition

#### Conf-file option
- [x] New cmd line option to specify a conf file to be parsed (instead of default $HOME/.conf/clight.conf)
- [x] Fix small conf reading bug

#### Long gamma transitions
- [x] Add a long_gamma_transition option, to change gamma in a way similar to redshift (very long fading trnasition)

#### Gamma fixes
- [x] Double check state.time + state.in_event usage in BACKLIGHT and user scripts
- [x] Test BACKLIGHT timeouts with verbose option

- [x] FIX: next_event should point to next event as soon as current event is elapsed (eg: SUNRISE 7:00, at 6:59 next_event -> SUNRISE, at 7:00 next_event -> SUNSET)
- [x] FIX gamma long transitions for long suspend cycles (eg: put laptop to sleep during NIGHT and awake it during DAY)

#### Location fixes
- [x] 0.0:0.0 is actually a good location. Avoid using these values to filter away unsetted location
- [x] fix: ->  WARN("Wrong longitude value. Resetting default value.\n");

#### Generic
- [x] Update wiki with new bus api
- [x] Update conf file/clight autocompletion script/interface.c with new options
- [x] Add Cpack support to cmake
- [x] Add issue template
- [x] bump Clightd required version (release after clightd!)
- [x] Reset conf.event_duration default value (changed for testing)

### 3.3

#### DPMS
- [ ] Dpms should be a special case of screen dimming, ie: a Clightd.Idle client that waits on a timeout, and when IDLE signal is sent, it should switch off screen.
It should work on wayland(wlroots) too.

### 3.4

#### BACKLIGHT multiple-monitors curves
- [ ] Add support for config files to give each monitor its own backlight curves. Something like /etc/clight/clight.conf + /etc/clight/mon.d/$MONITOR_SERIAL.conf (where MONITOR_SERIAL can be found through org.clightd.clightd.Backlight.GetAll)
- [ ] If any conf file is found in /etc/clight/mon.d/, avoid calling SetAll, and just call Set on each serial.

### 3.X/4.0
- [ ] Port to libmodule
