## 4.11

## 5.0

### Generic
- [ ] drop old config file path support (OLDCONFDIR)

### Backlight
- [ ] Drop is_smooth option (no need, just specify a step/wait > 0)
- [ ] Move "Capture","IncBl", "DecBl" interface methods to org.clight.clight.Conf.Backlight

### Gamma
- [ ] Drop is_smooth option (no need, just specify a step/wait > 0)

### Dimmer
- [ ] Drop is_smooth option (no need, just specify a step/wait > 0)

### Sensor
- [ ] Allow multiple sensors to be specified in priority order; those sensors will be stored in a list and the first available will be used.
- [ ] Changes: -> is_sensor_available() that will be called on each of the listed sensors; first avaiable will become highest_prio_available_dev_name
- [ ] capture_frames_brightness -> will use highest_prio_available_dev_name

## Future

### Pinephone
- [ ] Finalize support

### Generic
- [ ] Port to libmodule 6.0.0 (?)
- [ ] Add a Dump dbus method (and a DUMP_REQ request) to allow any module to dump their state (module_dump()) to a txt file
