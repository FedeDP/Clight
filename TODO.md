## 1.4
### Weather
- [x] add weather support -> New struct for timeouts wuld be something like conf.timeout[enum state][enum weather] where enum weather = { UNWKNOWN, SUNNY, RAINY, CLOUDY } and defaults to 0 obviously -> state.weather = 0; ...or just use something like conf.temp[state.time] that cuts up to 50% at 100% cloudiness. Weather support to use conf.apikey and each user has to set his apikey there
- [x] make it optional at build time (libjansson and libcurl too...or use socket and strstr?)
- [ ] add a network module that listens on networkmanager connection events and starts weather as soon as a connection is available
- [ ] weather should depend on network module
- [x] brightness should depend on weather (soft)
- [x] conf.weather_timeout[ON_AC/ON_BATT] -> timeout between weather refresh
- [x] weather should call reset_timer on BRIGHTNESS when updating cloudiness informations (but reset_timer on BRIGHTNESS should call get_weather_aware_timeout() even when called by GAMMA/other modules)
- [x] fix: gamma/gamma smooth start twice only with weather module enabled.

### Log file rework
- [x] Do not print Latitude = 0.0 Longitude = 0.0 in log file if no user position is setted
- [x] Rework conf log (like conf file -> ## Module, ## Generic, ## Timeouts ecc ecc)
- [x] Fix log: is_started_disabled instead of is_disabled?

## Later/Ideas
- [ ] subscribe to "interfaceEnabledChanged" signal from clightd (as soon as it is implemented in clightd) and do a capture as soon as interface became enabled (May be disable both dimmer and brightness while interface is disabled)
