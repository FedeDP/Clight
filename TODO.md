## High Priority - before merge in master
- [x] if conf.sunrise and sunset are both setted, disable_module(LOCATION_IX)
- [x] avoid calling set_self_deps if module is disabled in set_xxx_self()

## Mid Priority:
- [ ] add an initial setup to ask user to eg: set desired screen backlight level matching current ambient brightness, max brightess captured from webcam (eg: ask him to switch on a torch on webcam lens), and min brightness captured (ask him to cover the webcam). Moreover, set lowest backlight level and ask user if it can see (sometimes at 0 backlight display gets completely dimmed off)
- [x] add a module dependency management system (lazy module loading)
- [x] move "bus" as module

## Low Priority:
- [x] follow https://github.com/systemd/systemd/issues/5654 (next systemd release?) (need to make a PR upstream) [PR merged] . Add a check on #if LIBSYSTEMD_VERSION >= 234 and use new exposed function in bus_call (sd_bus_message_appendv())
- [ ] add weather support (WIP) New struct for timeouts wuld be something like conf.timeout[enum state][enum weather] where enum weather = { UNWKNOWN, SUNNY, RAINY, CLOUDY } and defaults to 0 obviously -> state.weather = 0; ...or just use something like conf.temp[state.time] that cuts up to 50% at 100% cloudiness 
- [ ] cache latest location retrieved to be taken next time clight starts if geoclue does not give us any location (eg: no/poor internet connection)

