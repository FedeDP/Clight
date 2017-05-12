## 0.12
- [ ] load brightness curve coefficients from a file and use libgsl to perform a regression on them.
In config file, add an array of floats -> [0, 1.4, 2.3, ...] . Then malloc(config_setting_length) parameters and store inside it each config_setting_get_elem(config_setting_get_member("floats"), i).
Finally, do a regression with gsl on these parameters and store new curve parameters.
- [ ] cache latest location retrieved to be taken next time clight starts if geoclue does not give us any location (eg: no/poor internet connection) -> use a timerfd on LOCATION module: if in 5s (or less?) geoclue2 does not provide us any location, load from cache.
- [x] make libconfig optional

## 0.13
- [ ] add a backlight dimmer module

## Later
- [ ] add weather support -> New struct for timeouts wuld be something like conf.timeout[enum state][enum weather] where enum weather = { UNWKNOWN, SUNNY, RAINY, CLOUDY } and defaults to 0 obviously -> state.weather = 0; ...or just use something like conf.temp[state.time] that cuts up to 50% at 100% cloudiness (mid)
