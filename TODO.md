## 0.15
- [x] specify regression points for both AC and BATTERY? This would remove the need for -batt_max_backlight_pct=INT option and would be a really nice feature
- [x] only change brightness if backlight_sysfs has /sys/class/backlight/*/device/enabled == "enabled" (maybe do this check in clightd, only if a syspath is not passed though, otherwise avoid this check) -> eg: when using external monitor in my case, i got device/enabled == "disabled". It is useless to change brightness on a disabled device.
- [x] drop libconfig optionality (some settings are only available through config file)
- [x] in case of EHOSTUNREACH (#define EHOSTUNREACH    113 /* No route to host */) error from busctl call, do not leave (and drop if (state.quit) { state.quit = 0 }  in location and upower modules)
- [x] use setjmp before entering main_poll and use a longjmp in case of critical error (to avoid ugly nested ifs)
- [x] Leave for EHOSTUNREACH error related only to org.clightd.backlight
- [x] Dimmer module too should not touch brightness if interface is not enabled
- [x] FIX: gamma is started before brightness, but brightness does not wait for gamma_cb to be started??
- [x] FIX: no-dpms makes Location module load loc from cache

## 0.16
- [ ] add a CRITICAL_ON_BATT level to customize even more various timeouts/actions
- [ ] dimmer module smooth transitions

## Later
- [ ] add weather support -> New struct for timeouts wuld be something like conf.timeout[enum state][enum weather] where enum weather = { UNWKNOWN, SUNNY, RAINY, CLOUDY } and defaults to 0 obviously -> state.weather = 0; ...or just use something like conf.temp[state.time] that cuts up to 50% at 100% cloudiness
- [ ] improve (amd simplify) modules' handling
