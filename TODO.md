## 0.12
- [x] compute polynomial best fit on pairs of index -> value
- [x] load brightness curve coefficients from a file 
- [x] update readme
- [x] update PKGBUILD/debian control with gsl new dep
- [x] cache latest location retrieved to be taken next time clight starts if geoclue does not give us any location (eg: no/poor internet connection) -> use a timerfd on LOCATION module: if in 5s (or less?) geoclue2 does not provide us any location, load from cache.
- [x] make libconfig optional
- [x] fix popt memleak
- [x] fix issue with double free or corruption in disable_module
- [x] fix issue with some WMs not exporting XDG_SESSION_TYPE (switch to Xauthority that is required btw)

## 0.13
- [ ] add a backlight dimmer module

## Later
- [ ] add weather support -> New struct for timeouts wuld be something like conf.timeout[enum state][enum weather] where enum weather = { UNWKNOWN, SUNNY, RAINY, CLOUDY } and defaults to 0 obviously -> state.weather = 0; ...or just use something like conf.temp[state.time] that cuts up to 50% at 100% cloudiness (mid)
