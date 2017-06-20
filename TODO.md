## 0.14
- [ ] specify regression points for both AC and BATTERY? This would remove the need for -batt_max_backlight_pct=INT option and would be a really nice feature
- [ ] only change brightness if backlight_sysfs has /sys/class/backlight/intel_backlight/device/enabled == "enabled" (maybe do this check in clightd, only if a syspath is not passed though, otherwise avoid this check) -> eg: when using external monitor in my case, i got device/enabled == "disabled". It is useless to change brightness on a disabled device.

## 0.15
- [ ] add a CRITICAL_ON_BATT level to customize even more various timeouts/actions?
- [ ] dimmer module smooth transitions!

## 0.16?
- [ ] add weather support -> New struct for timeouts wuld be something like conf.timeout[enum state][enum weather] where enum weather = { UNWKNOWN, SUNNY, RAINY, CLOUDY } and defaults to 0 obviously -> state.weather = 0; ...or just use something like conf.temp[state.time] that cuts up to 50% at 100% cloudiness
