## 0.12
- [ ] load brightness curve coefficients from a file and use libgsl to perform a regression on them.
In config file, add an array of floats -> [0, 1.4, 2.3, ...] . Then malloc(config_setting_length) parameters and store inside it each config_setting_get_elem(config_setting_get_member("floats"), i).
Finally, do a regression with gsl on these parameters and store new curve parameters.
- [ ] cache latest location retrieved to be taken next time clight starts if geoclue does not give us any location (eg: no/poor internet connection) -> use a timerfd on LOCATION module: if in 5s (or less?) geoclue2 does not provide us any location, load from cache.

## Later
- [ ] add an initial setup to ask user to eg: set desired screen backlight level matching current ambient brightness, max brightess captured from webcam (eg: ask him to switch on a torch on webcam lens), and min brightness captured (ask him to cover the webcam). Moreover, set lowest backlight level and ask user if it can see (sometimes at 0 backlight display gets completely dimmed off) (mid/Needed?)
- [ ] add weather support -> New struct for timeouts wuld be something like conf.timeout[enum state][enum weather] where enum weather = { UNWKNOWN, SUNNY, RAINY, CLOUDY } and defaults to 0 obviously -> state.weather = 0; ...or just use something like conf.temp[state.time] that cuts up to 50% at 100% cloudiness (mid)
- [ ] add a backlight dimmer module
