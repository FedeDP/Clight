## 4.3

### Generic
- [ ] Port to libmodule 6.0.0 (?)
- [ ] Bugfix: fix #106 (?)
- [ ] Add a Dump dbus method (and a DUMP_REQ request) to allow any module to dump their state (module_dump()) to a txt file

### BACKLIGHT multiple-monitors curves
- [ ] Add support for config files to give each monitor its own backlight curves. Something like /etc/clight/clight.conf + /etc/clight/mon.d/$MONITOR_SERIAL.conf (where MONITOR_SERIAL can be found through org.clightd.clightd.Backlight.GetAll)
- [ ] If any conf file is found in /etc/clight/mon.d/, avoid calling SetAll, and just call Set on each serial.

### KEYBOARD dimming
- [ ] Add support for keyboard dimming timeout...PR to Upower?
udevadm info --path /sys/class/leds/dell::kbd_backlight --a

looking at device '/devices/platform/dell-laptop/leds/dell::kbd_backlight':
KERNEL=="dell::kbd_backlight"
SUBSYSTEM=="leds"
DRIVER==""
ATTR{trigger}=="[none] usb-gadget usb-host rfkill-any rfkill-none kbd-scrolllock kbd-numlock kbd-capslock kbd-kanalock kbd-shiftlock kbd-altgrlock kbd-ctrllock kbd-altlock kbd-shiftllock kbd-shiftrlock kbd-ctrlllock kbd-ctrlrlock AC-online BAT0-charging-or-full BAT0-charging BAT0-full BAT0-charging-blink-full-solid disk-activity disk-read disk-write ide-disk mtd nand-disk cpu cpu0 cpu1 cpu2 cpu3 cpu4 cpu5 cpu6 cpu7 cpu8 cpu9 cpu10 cpu11 cpu12 cpu13 cpu14 cpu15 panic mmc0 bluetooth-power hci0-power rfkill0 audio-mute audio-micmute phy0rx phy0tx phy0assoc phy0radio rfkill1 "
ATTR{stop_timeout}=="10s"
ATTR{start_triggers}=="+keyboard +touchpad"
ATTR{max_brightness}=="2"
ATTR{brightness_hw_changed}=="0"
ATTR{brightness}=="0"

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

