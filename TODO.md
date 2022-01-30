## 4.9 (5.0?)

### Generic
- [ ] Fix MODULE_WITH_PAUSE modules: properly start in "paused" state if pause condition is met as soon as module starts

### Sensor
- [ ] Allow multiple sensors to be specified in priority order; those sensors will be stored in a list and the first available will be used.
- [ ] Changes: -> is_sensor_available() that will be called on each of the listed sensors; first avaiable will become highest_prio_available_dev_name
- [ ] capture_frames_brightness -> will use highest_prio_available_dev_name

### Backlight
- [x] Support content based backlight calibration
- [x] Fix/improve set_new_backlight() from backlight and screen
- [x] Fix: wait on state.screen_br update before starting, so that we can start with content-based if enabled

### Screen
- [x] start paused when starting with a timeout < 0
- [x] Drop old "contrib" based SCREEN tool, and just keep content-backlight behavior?
- [x] This would mean that if screen is not disabled, content-backlight is implicit
- [x] Better manage clogged state
- [x] New msg: SCREEN_UPD -> emit screen_br updates
- [x] Expose state.screen_br in interface
- [x] Drop state.content_based?

### KbdBacklight
- [x] Port to use BL_UPD instead of ambient_upd: this way it works fine while content_based backlight is enabled, and follows monitor specific_curves and ambient gamma rule (ie: use default backlight curve as target)
- [x] Better still: it allows KEYBOARD to react to external backlight changes too
- [ ] TEST -> bl_upd when smoothing (may be ok -> smoother transitions) or becoming dimmed (should be ok as there is already an option to pause kbd when dimmed)
- [x] dim by default and drop config (why was it there in the first place?)

### Backlight
- [ ] Drop is_smooth option (no need, just specify a step/wait > 0)

### Gamma
- [ ] Drop is_smooth option (no need, just specify a step/wait > 0)

### Dimmer
- [ ] Drop is_smooth option (no need, just specify a step/wait > 0)

## Future

### Pinephone
- [ ] Finalize support

### Generic
- [ ] Port to libmodule 6.0.0 (?)
- [ ] Add a Dump dbus method (and a DUMP_REQ request) to allow any module to dump their state (module_dump()) to a txt file
