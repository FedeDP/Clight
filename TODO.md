## Mid Priority:
- [ ] add an initial setup to ask user to eg: set desired screen backlight level matching current ambient brightness, max brightess captured from webcam (eg: ask him to switch on a torch on webcam lens), and min brightness captured (ask him to cover the webcam). Moreover, set lowest backlight level and ask user if it can see (sometimes at 0 backlight display gets completely dimmed off) (mid...needed?)
- [ ] get_timeout function to retrieve timerfd remaining timer (easy)
- [ ] update clight checking if EPERM is returned from bus calls to clightd (it means this clight session is no more active). Do not fail.

## Low Priority:
- [ ] cache latest location retrieved to be taken next time clight starts if geoclue does not give us any location (eg: no/poor internet connection)
- [x] make dpms optional
- [ ] make event_duration customizable too (easy)

## Ideas
- [ ] Upower battery/ac signals monitor? Ie: add a match on bus. May be more frequent captures if on AC, and less frequent on battery? (easy)
- [ ] add weather support -> New struct for timeouts wuld be something like conf.timeout[enum state][enum weather] where enum weather = { UNWKNOWN, SUNNY, RAINY, CLOUDY } and defaults to 0 obviously -> state.weather = 0; ...or just use something like conf.temp[state.time] that cuts up to 50% at 100% cloudiness (mid)
