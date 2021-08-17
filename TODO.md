## 4.7

### Keyboard
- [x] Add curve configuration
- [x] Use curve config
- [x] drop ambient_threshol
- [x] Expose curve points from dbus (read only)
- [x] allow to update them from dbus

### Gamma
- [x] Stop gamma where not supported

### Location
- [x] Location getClient async to avoid waiting 20s on startup
- [x] Send a ping to check if geoclue exists
- [x] add a timeout like 30s of waiting on geoclue; if no location is received, kill it and go on 
(this avoids weird situations where there is no cached location and clight awaits forever for geoclue to give a location); print a meaningful warning!

### Interface
- [x] Expose conf.monitor_override dbus api (List()"a(sadad)", Set("sadad"), Set(NULL))
- [x] Document new api (https://github.com/FedeDP/Clight/wiki/Api#orgclightclightconfmonitoroverride)

### Generic
- [x] evaluate using module_deregister instead of m_poisonpill?
- [x] document in wiki pages DBus api the new Restore option for gamma and backlight
- [x] Uniform stdout and log file logs
- [x] Plot backlight curves directly using fit parameters (makes much more sense)
- [x] Plot backlight curve for any curve (backlight, monitor_override and keyboard) only in verbose mode

## 4.8

### Backlight
- [ ] Port to clightd.Backlight2 API
- [ ] Drop screen_path bl conf
- [ ] Drop is_smooth option (no need, just specify a step/wait > 0)

## 4.x
- [ ] Port to libmodule 6.0.0 (?)
- [ ] Add a Dump dbus method (and a DUMP_REQ request) to allow any module to dump their state (module_dump()) to a txt file


