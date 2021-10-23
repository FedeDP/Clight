## 4.8

### Pinephone
- [x] Allow upower module even if LidIsPresent is false to properly support pinephone

### Interface
- [x] Fixed null ptr dereference in inhibit_parse_msg()

### Generic
- [x] Finally allow to view usage, help and version messages while another instance is running. Fixes #235
- [x] Show commit hash in clight version

### Backlight
- [x] Match new objectadded/removed signal from Backlight2 clightd api
- [x] on objectadded, properly set current_bl_pct + current_temp on new monitor
- [x] Keep a map of currently available monitors and use this instead of calling GetAll when setting monitor override curves
- [x] Drop screen_path bl conf
- [x] Drop BacklightSyspath from dbus API wiki
- [x] Drop backlight from autocompletions script and man page
- [x] Port to clightd.Backlight2 API
- [x] add faq entry "no external monitor found"

### Wizard
- [x] Port to backlight2

## 4.x

### Generic
- [ ] Port to libmodule 6.0.0 (?)
- [ ] Add a Dump dbus method (and a DUMP_REQ request) to allow any module to dump their state (module_dump()) to a txt file


## After Clightd API break:

### Backlight
- [ ] Drop is_smooth option (no need, just specify a step/wait > 0)

### Gamma
- [ ] Drop is_smooth option (no need, just specify a step/wait > 0)
