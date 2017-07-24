## 1.1
- [x] understand issue with get_gamma_events
- [x] fix issue with dimmer module
- [x] is get_screen_dpms() still needed? Ie: if screen is in dpms mode, backlight interface won't be enabled for sure. (setting a longer timeout while on dpms is not a huge optimization and we can probably remove it)
- [x] log single capture mode output too (clight_capture.log)
- [x] fix dimmer smooth transition: sd_bus does not support multithread -> let modules define more than a single fd to be polled (so we can poll both dimmer timerfd, dimmer inotify and dimmer smooth transition, and gamma too can make use of that, by splitting smooth transitioning), or use 2 different modules
- [x] add a conf.dim_smooth_transition and a conf.gamma_smooth_transition
- [x] improve code
- [x] get_current_brightness in dim_backlight is wrong: we have to call it only first time, and then smoothing transition on it; otherwise when leavin dimmed state, we would restore wrong backlight
- [x] better defaults for dimmer and dpms
- [x] runtime switch for debug
- [x] improved opts (use "--x-y" instead of "--x_y" for multiwords options)
- [x] move quit_buf inside state struct
- [x] get_timeout should let decide which field to return (tv_sec or tv_nsec)
- [x] FIX: dimmer_smooth module (and gamma_smooth_module too) is started after DIMMER receives first poll cb. This is wrong, it should be started as soon as dimmer(gamma) is started.
- [x] port gamma smooth transition to same module as dimmer smooth transition?
- [x] add dimmer transitions even when leaving dimmed state
- [x] port onGeoclueNewEvent to same format as onUpowerNewEvent (with callbacks registered in init())
- [x] update module.skel
- [x] check FIXME!
- [x] unify add_x_bus_callback and run_callbacks() under same interface in bus.c?
- [x] rename --debug to --verbose and add a --version / -v switch
- [x] use *userdata parameter in add_match instead of a global state variable (state.bus_cb_idx)
- [x] switch to SD_BUS_TYPE_* in bus.c
- [x] test --no-dpms, --no-dimmer, --no-gamma -> --no-dimmer disables brightness module
- [x] update to new clightd captureframes interface
- [x] Use modules[X].disabled = 1 instead of conf.no_gamma, no_dimmer, no-dpms etc etc
- [x] smarter init_modules() -> avoid in various check() functions call to conf.single_capture_mode. 
- [x] FIXME: dimmer module gets disabled as soon as dimmer-smooth module is disabled as it is only module dependent
- [x] add a modules[m].state in module struct and an enum module_states { UNKNOWN, DISABLED, INTIED }
- [x] if module gets inited, if needed call its various callbacks setter for UPOWER, GEOCLUE(add_mod_callback) (eventually both, use a variadic parameter)
- [x] add a fake X module required by modules who needs X server running (to avoid check() to call !state.display || !state.xauthority)
- [ ] drop "standalone" module self_t and instead add a Clight target (fake module, like Xorg one) that requires needed modules (toplevel) : {HARD, BUS}, {SOFT, GAMMA}, {SOFT, DIMMER}, {HARD, BRIGHTNESS}, {HARD, SIGNAL}

## 1.2
- [ ] subscribe to "interfaceEnabledChanged" signal from clightd (as soon as it is implemented in clightd) and do a capture as soon as interface became enabled (May be disable both dimmer and brightness while interface is disabled)

## Later
- [ ] add weather support -> New struct for timeouts wuld be something like conf.timeout[enum state][enum weather] where enum weather = { UNWKNOWN, SUNNY, RAINY, CLOUDY } and defaults to 0 obviously -> state.weather = 0; ...or just use something like conf.temp[state.time] that cuts up to 50% at 100% cloudiness
- [ ] add a CRITICAL_ON_BATT level to customize even more various timeouts/actions
