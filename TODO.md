## 0.14

### Backlight dimmer module
- [ ] avoid using Xlib functions as module has to work on both x and wayland!! If not possible, add proper check in check() function!
- [x] timerfd with duration N seconds. When timer ends, check afk time
- [x] When we enter dimmed mode, add an inotify watcher on /dev/input folder; dim screen backlight.
- [x] When inotify watcher wakes up (receives an event on /dev/input), we have to leave dimmed mode. Destroy watcher and dimmed = false
- [ ] Dimmed = true -> brightness module won't change brightness until screen is dimmed (but will do capture anyway until eventually dpms kicks in. This way, when we leave dimmed state, we can set correct backlight level for current brightness)
- [ ] when we enter dimmed state, lower backlight level
- [ ] when we leave dimmed state, set latest backlight level.
- [ ] add conf.dimmer_timeout[ON_AC/ON_BATT] to both cmdline and conf file. Defaults to 5min, 2min?

## Later
- [ ] add weather support -> New struct for timeouts wuld be something like conf.timeout[enum state][enum weather] where enum weather = { UNWKNOWN, SUNNY, RAINY, CLOUDY } and defaults to 0 obviously -> state.weather = 0; ...or just use something like conf.temp[state.time] that cuts up to 50% at 100% cloudiness (mid)
