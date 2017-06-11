## 0.14

### Backlight dimmer module
- [x] add proper check in check() function for X11
- [x] make libx11 optdep
- [x] timerfd with duration N seconds. When timer ends, check afk time
- [x] When we enter dimmed mode, add an inotify watcher on /dev/input folder; dim screen backlight.
- [x] When inotify watcher wakes up (receives an event on /dev/input), we have to leave dimmed mode. Destroy watcher and dimmed = false
- [x] Dimmed = true -> brightness module won't do any capture
- [x] when we enter dimmed state, lower backlight level (only if current backlight level is > conf.dim_pct)
- [x] when we leave dimmed state, reset timer on brightness module to do a capture and set correct new backlight level
- [x] add conf.dimmer_timeout[ON_AC/ON_BATT] and conf.dim_pct to both cmdline and conf file. Defaults to 5min, 2min, 20%?
- [x] add a conf.no_dimmer config option as not everybody may wish a dimmer inside clight
- [x] fix bug while consuming inotify data...
- [x] fix: when leaving dimmed state, set backlight level used before entering dimmed state and check if it is needed to do a capture (ie: store last backlight level and re-set it in case a capture is not needed)
- [x] fix: check if 20% of max backlight level is > current backlight level and if true, do not touch current backlight
- [x] fix travis build
- [x] move libx11 -> getIdleTime support to clightd and drop libx11 requirement in clight
- [ ] FIXME: Upower should restart dimmer module timer on ac state changes too (find a way to make upower restart correct timers: modules dependent on it)
- [ ] FIXME: upower call to poll_cb in init is wrong because it assumes that its dependent modules have already been inited.


## Later
- [ ] add weather support -> New struct for timeouts wuld be something like conf.timeout[enum state][enum weather] where enum weather = { UNWKNOWN, SUNNY, RAINY, CLOUDY } and defaults to 0 obviously -> state.weather = 0; ...or just use something like conf.temp[state.time] that cuts up to 50% at 100% cloudiness
