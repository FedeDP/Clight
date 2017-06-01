## 0.14

### Backlight dimmer module
- [ ] avoid using Xlib functions as module has to work on both x and wayland!!
- [ ] timerfd with duration conf.dimmer_timeout. When timer ends, check /dev/input folder for seconds of inactivity. If idle time is == conf.dimmer timeout, enter dimmed mode. Else, set a timeout of conf.dimmer_timeout - idle time. -> ISSUE: stat /dev/input/ won't return correct time for events. Logind? busctl get-property org.freedesktop.login1 /org/freedesktop/login1/session/c1 org.freedesktop.login1.Session IdleSinceHint -> always returns 0 on kde...may be on Gnome it is correctly implemented? ...libinput?
- [ ] When we enter dimmed mode, add an inotify watcher on /dev/input folder; dim screen backlight. Dimmed = true -> brightness module won't change brightness until screen is dimmed.
- [ ] When inotify watcher wakes up (receives an event on /dev/input), we have to leave dimmed mode. Destroy watcher and dimmed = false

## Later
- [ ] add weather support -> New struct for timeouts wuld be something like conf.timeout[enum state][enum weather] where enum weather = { UNWKNOWN, SUNNY, RAINY, CLOUDY } and defaults to 0 obviously -> state.weather = 0; ...or just use something like conf.temp[state.time] that cuts up to 50% at 100% cloudiness (mid)
