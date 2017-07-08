## 1.1
- [x] understand issue with get_gamma_events
- [x] fix issue with dimmer module
- [x] is get_screen_dpms() still needed? Ie: if screen is in dpms mode, backlight interface won't be enabled for sure. (setting a longer timeout while on dpms is not a huge optimization and we can probably remove it)
- [x] log single capture mode output too (clight_capture.log)
- [ ] dimmer module smooth transitions

## 1.2
- [ ] subscribe to "interfaceEnabledChanged" signal from clightd (as soon as it is implemented in clightd) and do a capture as soon as interface became enabled (May be disable both dimmer and brightness while interface is disabled)

## Later
- [ ] add weather support -> New struct for timeouts wuld be something like conf.timeout[enum state][enum weather] where enum weather = { UNWKNOWN, SUNNY, RAINY, CLOUDY } and defaults to 0 obviously -> state.weather = 0; ...or just use something like conf.temp[state.time] that cuts up to 50% at 100% cloudiness
- [ ] improve (and simplify) modules' handling?
- [ ] add a CRITICAL_ON_BATT level to customize even more various timeouts/actions
