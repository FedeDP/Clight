## 4.8

### Pinephone

- [x] Allow upower module even if LidIsPresent is false to properly support pinephone

## 4.x

### Generic

- [ ] Port to libmodule 6.0.0 (?)
- [ ] Add a Dump dbus method (and a DUMP_REQ request) to allow any module to dump their state (module_dump()) to a txt file

### Backlight
- [ ] Port to clightd.Backlight2 API
- [ ] Drop screen_path bl conf
- [ ] Drop is_smooth option (no need, just specify a step/wait > 0)

