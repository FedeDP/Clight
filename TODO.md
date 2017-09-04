## 1.4
### Weather
- [x] add weather support -> New struct for timeouts wuld be something like conf.timeout[enum state][enum weather] where enum weather = { UNWKNOWN, SUNNY, RAINY, CLOUDY } and defaults to 0 obviously -> state.weather = 0; ...or just use something like conf.temp[state.time] that cuts up to 50% at 100% cloudiness. Weather support to use conf.apikey and each user has to set his apikey there
- [ ] make it optional at build time (libjansson and libcurl too...or use socket and strstr?)
- [ ] add a network module that listens on networkmanager connection events and starts weather as soon as a connection is available
- [ ] weather should depend on network module
- [x] brightness should depend on weather (soft)
- [x] conf.weather_timeout[ON_AC/ON_BATT] -> timeout between weather refresh
- [ ] fix: gamma/gamma smooth start twice only with weather module enabled.
Next gamma alarm due to: Sat Sep  2 19:30:00 2017
Next gamma alarm due to: Sat Sep  2 19:30:00 2017
Gamma temp was already 6500
Gamma temp was already 6500

### Log file rework
- [ ] Do not print Latitude = 0.0 Longitude = 0.0 in log file if no user position is setted
- [ ] Rework conf log (like conf file -> ## Module, ## Generic, ## Timeouts ecc ecc)
- [x] Fix log: is_started_disabled instead of is_disabled?

## Later/Ideas
- [ ] subscribe to "interfaceEnabledChanged" signal from clightd (as soon as it is implemented in clightd) and do a capture as soon as interface became enabled (May be disable both dimmer and brightness while interface is disabled)
