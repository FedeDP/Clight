## 1.2
- [ ] rework conf settings: timeouts should be: AC_capture_timeouts = { 600, 2700, 300 } BATT_capture_timeouts = {  } 
- [x] autodisabling of bus-dependent module seems broken now: it seems sd_bus_add_match returns 1 even if interface is not existent
- [x] start clight after org.freedesktop.PowerManagement.Inhibit interface? (it seems that interface is still missing when clight starts) -> if this can't be fixed, add a systemd timer that starts clight 5s later

## 1.3
- [ ] subscribe to "interfaceEnabledChanged" signal from clightd (as soon as it is implemented in clightd) and do a capture as soon as interface became enabled (May be disable both dimmer and brightness while interface is disabled)

## Later
- [ ] add weather support -> New struct for timeouts wuld be something like conf.timeout[enum state][enum weather] where enum weather = { UNWKNOWN, SUNNY, RAINY, CLOUDY } and defaults to 0 obviously -> state.weather = 0; ...or just use something like conf.temp[state.time] that cuts up to 50% at 100% cloudiness
- [ ] add a CRITICAL_ON_BATT level to customize even more various timeouts/actions
