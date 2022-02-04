## 4.9

### Generic
- [x] automatic config parser/dumper: use X_Macro to declare config fields
- [x] Possibly same for log_conf().
- [x] Fix conf namings that now must match clight.conf namings
- [x] Fix bl_smooth_t for backlight too (?) See validate_backlight()
- [ ] Implement for simple arrays too (? is this feasible?)

- [ ] fix check_X_conf() ? May be specify value ranges in X_CONF macro?

### Pinephone
- [ ] Finalize support

### Sensor
- [ ] Allow multiple sensors to be specified in priority order; those sensors will be stored in a list and the first available will be used.
- [ ] Changes: -> is_sensor_available() that will be called on each of the listed sensors; first avaiable will become highest_prio_available_dev_name
- [ ] capture_frames_brightness -> will use highest_prio_available_dev_name

## After Clightd API break:

### Backlight
- [ ] Drop is_smooth option (no need, just specify a step/wait > 0)

### Gamma
- [ ] Drop is_smooth option (no need, just specify a step/wait > 0)
