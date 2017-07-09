## 1.1
- [x] understand issue with get_gamma_events
- [x] fix issue with dimmer module
- [x] is get_screen_dpms() still needed? Ie: if screen is in dpms mode, backlight interface won't be enabled for sure. (setting a longer timeout while on dpms is not a huge optimization and we can probably remove it)
- [x] log single capture mode output too (clight_capture.log)
- [ ] fix dimmer smooth transition: sd_bus does not support multithread -> let modules define more than a single fd to be polled (so we can poll both dimmer timerfd, dimmer inotify and dimmer smooth transition, and gamma too can make use of that, by splitting smooth transitioning)
- [x] add a conf.dim_smooth_transition and a conf.gamma_smooth_transition
- [x] improve code
- [x] get_current_brightness in dim_backlight is wrong: we have to call it only first time, and then smoothing transition on it; otherwise when leavin dimmed state, we would restore wrong backlight
- [x] better defaults for dimmer and dpms
- [x] runtime switch for debug
- [x] improved opts (use "--x-y" instead of "--x_y" for multiwords options)

## 1.2
- [ ] subscribe to "interfaceEnabledChanged" signal from clightd (as soon as it is implemented in clightd) and do a capture as soon as interface became enabled (May be disable both dimmer and brightness while interface is disabled)

## Later
- [ ] add weather support -> New struct for timeouts wuld be something like conf.timeout[enum state][enum weather] where enum weather = { UNWKNOWN, SUNNY, RAINY, CLOUDY } and defaults to 0 obviously -> state.weather = 0; ...or just use something like conf.temp[state.time] that cuts up to 50% at 100% cloudiness
- [ ] add a CRITICAL_ON_BATT level to customize even more various timeouts/actions
