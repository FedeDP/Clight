## 4.2

### Generic
- [x] Fix keyboard backlight calibration
- [x] Fix wizard mode
- [x] Fix clight desktop file
- [x] Update wiki pages
- [x] Avoid geoclue continuosly spamming positions to clight
- [ ] On slow resume from suspend, weird things happen:
(D)[08:31:20]{timer.c:38}       Set timeout of 5s 0ns on fd 47.
(I)[08:31:20]{daytime.c:190}    Next alarm due to: Thu Nov 12 16:25:00 2020
(D)[08:31:20]{timer.c:38}       Set timeout of 28420s 0ns on fd 46.
(D)[08:31:20]{bus.c:175}        get_screen_brightness(): Input/output error
(D)[08:31:20]{timer.c:38}       Set timeout of 30s 0ns on fd 45.
(D)[08:31:20]{bus.c:175}        capture_frames_brightness(): Operation not permitted
(D)[08:31:20]{timer.c:38}       Set timeout of 600s 0ns on fd 48.
(I)[08:31:20]{test.c:17}        We're now during the day!
(D)[08:31:20]{interface.c:436}  Emitting DayTime property
(D)[08:31:20]{timer.c:38}       Set timeout of 0s 1ns on fd 48.
(D)[08:31:20]{interface.c:436}  Emitting NextEvent property
(D)[08:31:20]{bus.c:175}        capture_frames_brightness(): Operation not permitted
(D)[08:31:20]{timer.c:38}       Set timeout of 600s 0ns on fd 48.
(I)[08:31:20]{upower.c:52}      AC cable disconnected.
(I)[08:31:20]{upower.c:63}      Laptop lid opened.
(D)[08:31:20]{interface.c:436}  Emitting AcState property
(D)[08:31:20]{timer.c:38}       Set timeout of 0s 1ns on fd 48.
(D)[08:31:20]{timer.c:40}       Disarmed timerfd on fd 45.
(D)[08:31:20]{validations.c:18} Failed to validate upower request.
(D)[08:31:20]{interface.c:436}  Emitting LidState property
(D)[08:31:20]{validations.c:143}        Failed to validate lid request.
(D)[08:31:20]{bus.c:175}        capture_frames_brightness(): Operation not permitted
(D)[08:31:20]{timer.c:38}       Set timeout of 1200s 0ns on fd 48.
(I)[08:31:25]{gamma.c:139}      Normal transition to 6500 gamma temp.

* Easy solution: add a global delay_on_lid_opened option that just sleeps N seconds on lid opened; check

### Backlight
- [x] Add an option to fire a calibration whenever lid gets opened

### Dpms
- [x] Hook to new Clightd 4.3 Dpms.Changed signal to react to external (through clightd) dpms state changes

### Gamma
- [x] Hook to new Clightd 4.3 Gamma.Changed signal to react to external (through clightd) gamma state changes
- [x] Fix gamma not setting correct temp on resume -> clight is too quick on resume, and X is not yet loaded when it tries to set new screen temperature...
introduce a small timeout of 5s for gamma corrections
- [x] Allow users to customize the delay

### Keyboard
- [x] Fixed bug in set_keyboard_level()

### Clightd 5.0
- [x] Require clightd 5.0
- [x] Port to clightd new API for dpms signal
- [x] Clightd does now support gamma and dpms on wayland too (where corrispective protocols are supported)
- [x] Clightd does now support gamma on tty too!

## 4.3

### Generic
- [ ] Port to libmodule 6.0.0 (?)
- [ ] Bugfix: fix #106 (?)

### BACKLIGHT multiple-monitors curves
- [ ] Add support for config files to give each monitor its own backlight curves. Something like /etc/clight/clight.conf + /etc/clight/mon.d/$MONITOR_SERIAL.conf (where MONITOR_SERIAL can be found through org.clightd.clightd.Backlight.GetAll)
- [ ] If any conf file is found in /etc/clight/mon.d/, avoid calling SetAll, and just call Set on each serial.

### Bus
- [ ] Expose BUS_REQ to make dbus call from custom modules
