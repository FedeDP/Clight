## Mid Priority:
- [ ] add an initial setup to ask user to eg: set desired screen backlight level matching current ambient brightness, max brightess captured from webcam (eg: ask him to switch on a torch on webcam lens), and min brightness captured (ask him to cover the webcam). Moreover, set lowest backlight level and ask user if it can see (sometimes at 0 backlight display gets completely dimmed off)

## Low Priority:
- [ ] cache latest location retrieved to be taken next time clight starts if geoclue does not give us any location (eg: no/poor internet connection)

## Ideas
- [ ] Upower battery/ac signals monitor? Ie: add a match on bus. May be more frequent captures if on AC, and less frequent on battery?
- [ ] add weather support -> New struct for timeouts wuld be something like conf.timeout[enum state][enum weather] where enum weather = { UNWKNOWN, SUNNY, RAINY, CLOUDY } and defaults to 0 obviously -> state.weather = 0; ...or just use something like conf.temp[state.time] that cuts up to 50% at 100% cloudiness 
