## 4.7

### Keyboard
- [x] Add curve configuration
- [x] Use curve config
- [x] drop ambient_threshold
- [x] Expose curve points from dbus (read only)
- [x] allow to update them from dbus

### Gamma
- [x] Stop gamma where not supported

### Location
- [x] Location getClient async to avoid waiting 20s on startup

### Generic
- [ ] evaluate using m_stop instead of m_poisonpill?
- [x] document in wiki pages DBus api the new Restore option for gamma and backlight

## 4.8

### Backlight
- [ ] Port to clightd.Backlight2 API
- [ ] Drop screen_path bl conf
- [ ] Drop is_smooth option (no need, just specify a step/wait > 0)

## 4.x
- [ ] Port to libmodule 6.0.0 (?)
- [ ] Add a Dump dbus method (and a DUMP_REQ request) to allow any module to dump their state (module_dump()) to a txt file


