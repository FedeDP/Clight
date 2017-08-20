## 1.3
- [x] Fix check for <= 0 dimmer_timeout[state.ac_state] in dimmer module (to pause it)
- [x] FIX: bug in gamma -> if laptop gets sleeped after sunset (eg at 1.00pm) and resumed after sunrise (eg at 11am), it won't set 6500k

## Later/Ideas
- [ ] add weather support -> New struct for timeouts wuld be something like conf.timeout[enum state][enum weather] where enum weather = { UNWKNOWN, SUNNY, RAINY, CLOUDY } and defaults to 0 obviously -> state.weather = 0; ...or just use something like conf.temp[state.time] that cuts up to 50% at 100% cloudiness
- [ ] add a CRITICAL_ON_BATT level to customize even more various timeouts/actions
- [ ] subscribe to "interfaceEnabledChanged" signal from clightd (as soon as it is implemented in clightd) and do a capture as soon as interface became enabled (May be disable both dimmer and brightness while interface is disabled)
