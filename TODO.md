## Mid Priority:
- [ ] add an initial setup to ask user to eg: set desired screen backlight level matching current ambient brightness, max brightess captured from webcam (eg: ask him to switch on a torch on webcam lens), and min brightness captured (ask him to cover the webcam). Moreover, set lowest backlight level and ask user if it can see (sometimes at 0 backlight display gets completely dimmed off)
- [x] Improve clight startup times: no need to set first timerfd after 1s; 1ns is enough.
- [x] improve ambient brightness to backlight level mapping
- [x] add a conf.lowest_backlight_level to avoid completely darken the screen
- [x] add a module dependency management system (lazy module loading)
- [x] move "bus" as module
- [x] prepare new release (1.0 or 0.10?)

## Low Priority:
- [x] follow https://github.com/systemd/systemd/issues/5654 (next systemd release?) (need to make a PR upstream) [PR merged] . Add a check on #if LIBSYSTEMD_VERSION >= 234 and use new exposed function in bus_call (sd_bus_message_appendv())
- [ ] add weather support (WIP) New struct for timeouts wuld be something like conf.timeout[enum state][enum weather] where enum weather = { UNWKNOWN, SUNNY, RAINY, CLOUDY } and defaults to 0 obviously -> state.weather = 0; ...or just use something like conf.temp[state.time] that cuts up to 50% at 100% cloudiness 
- [ ] cache latest location retrieved to be taken next time clight starts if geoclue does not give us any location (eg: no/poor internet connection)

