## 4.2

### Generic
- [x] Fix keyboard backlight calibration
- [x] Fix wizard mode
- [x] Fix clight desktop file
- [x] Update wiki pages
- [x] Fix clightd autostart when called by clight (cannot find gamma)
- [x] Add a sd-bus hook on systemd-logind PrepareForSleep signal that will pause any feature module
- [x] On resume, honor conf.wakedelay before resuming modules
- [x] Rename wakedelay to resumedelay, and honor it even on interface/custom modules SUSPEND_REQ; this allows to properly suppor suspend/resume cycles on non-systemd systems: just add a script to invoke "Pause true" upon suspend, and "Pause false" upon resume
- [x] Expose a "Pause" dbus API to pause clight
- [x] Fixed longstanding bug with negative timeouts in reset_timer()
- [x] Actually check that timerfd_gettime() succeed before using its value
- [x] Fix wrong timeout set on resume after a long night?
- [x] Update api doc (ResumeDelay, Suspended, Pause)
- [x] Require clightd 5.0
- [x] Better management of modules "paused" state (unified); see SCREEN, BACKLIGHT, DIMMER, DPMS paused_state
- [x] Added a zsh completion script (#169)
- [x] Add support for tty in utils.c

### Backlight
- [x] Add an option to fire a calibration whenever lid gets opened
- [x] Actually plot AC/BATT curves for backlight, in wizard mode too.
- [x] Fix timeout on daytime_upd: MSG_TYPE() macro needs parenthesis
- [x] Actually emit bl steps

### Screen 
- [x] Does now work on wl and tty too

### Dpms
- [x] Hook to new Clightd 5.0 Dpms.Changed signal to react to external (through clightd) dpms state changes
- [x] oes now work on wayland too
- [x] Port to clightd new API for dpms Changed signal

### Gamma
- [x] Hook to new Clightd 5.0 Gamma.Changed signal to react to external (through clightd) gamma state changes
- [x] Does now work on tty and wayland too
- [x] Actually emit gamma steps 

### Keyboard
- [x] Fixed bug in set_keyboard_level()

### Inhibition
- [x] Fix inhibiton counter going crazy if lots of inhibit requests are quicly sent to clight
- [x] Switch to DECLARE_HEAP_MSG where needed: most notably: we may receive looots of spam from ScreenSaver dbus monitor/api

### Location
- [x] Avoid geoclue continuosly spamming positions to clight

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

