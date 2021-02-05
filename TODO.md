## 4.3

### Backlight
- [x] Clamp backlight values between first backlight curve[ac state] value and last one, ie: never get out of backlight curve's points

### Interface
- [x] Fix crash when writing Sunrise/Sunset dayconf properties from dbus
- [x] Remember that 'b' dbus type maps to int when reading from sd_bus_message. This fixes a weird stack-smashing crash.
- [x] Allow to reset Sunrise/Sunset conf evts from dbus api

### Keyboard
- [x] Switch to use new KEYBOARD api offered by Clightd
- [x] Add support for StopTimeout (keyboard dimming)
- [x] Use backlight instead of ambient brightness? Thus following backlight curve
- [x] Start keyboard waiting on UPOWER

### Gamma
- [x] Do not use state.current_bl_pct in ambient_callback() as it may still have old value (it is now updated in dbus match on_bl_changed, async)
- [x] Only call ambient_callback() on target bl changes, not step ones

### Generic
- [ ] Parse cmdline flags before everything else? (#174)

## 4.4

### Generic
- [ ] Port to libmodule 6.0.0 (?)
- [ ] Bugfix: fix #106 (?)
- [ ] Add a Dump dbus method (and a DUMP_REQ request) to allow any module to dump their state (module_dump()) to a txt file

### BACKLIGHT multiple-monitors curves
- [ ] Add support for config files to give each monitor its own backlight curves. Something like /etc/clight/clight.conf + /etc/clight/mon.d/$MONITOR_SERIAL.conf (where MONITOR_SERIAL can be found through org.clightd.clightd.Backlight.GetAll)
- [ ] If any conf file is found in /etc/clight/mon.d/, avoid calling SetAll, and just call Set on each serial.

### Bus
- [ ] Expose BUS_REQ to make dbus call from custom modules -> 
CALL: sd_bus_message_appendv(m, signature, args) + sd_bus_message_readv(m, signature, args);
SET_PROP: sd_bus_set_propertyv
GET_PROP; sd_bus_get_property() + sd_bus_message_readv()
typedef struct {
    enum bus_call_type type;
    va_list write_args;
    va_list read_args;
} bus_req;

