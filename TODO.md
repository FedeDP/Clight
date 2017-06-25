## 0.15
- [x] specify regression points for both AC and BATTERY? This would remove the need for -batt_max_backlight_pct=INT option and would be a really nice feature
- [x] only change brightness if backlight_sysfs has /sys/class/backlight/*/device/enabled == "enabled" (maybe do this check in clightd, only if a syspath is not passed though, otherwise avoid this check) -> eg: when using external monitor in my case, i got device/enabled == "disabled". It is useless to change brightness on a disabled device.
- [x] drop libconfig optionality (some settings are only available through config file)
- [x] in case of EHOSTUNREACH (#define EHOSTUNREACH    113 /* No route to host */) error from busctl call, do not leave (and drop if (state.quit) { state.quit = 0 }  in location and upower modules)
- [x] use setjmp before entering main_poll and use a longjmp in case of critical error (to avoid ugly nested ifs)
- [x] Leave for EHOSTUNREACH error related only to org.clightd.backlight
- [x] FIX: gamma is started before brightness, but brightness does not wait for gamma_cb to be started??
- [x] FIX: no-dpms makes Location module load loc from cache
- [ ] FIX: clight gives "(E) [bus.c:225] Connection timed out" when running from autostart... and it won't get properly killed when session leaves...
LOG:
     1  Clight
     2  Version: 0.14
     3  Time: Sun Jun 25 19:49:04 2017
     4
     5  Starting options:
     6  * Number of captures:           5
     7  * Daily timeouts:               AC 600  BATT 1200
     8  * Nightly timeout:              AC 2700 BATT 5400
     9  * Event timeouts:               AC 300  BATT 600
    10  * Webcam device:
    11  * Backlight path:
    12  * Daily screen temp:            6500
    13  * Nightly screen temp:          4000
    14  * Smooth transitions:           enabled
    15  * User latitude:                0.00
    16  * User longitude:               0.00
    17  * User setted sunrise:
    18  * User setted sunset:
    19  * Gamma correction:             enabled
    20  * Min backlight level:          0
    21  * Event duration:               1800
    22  * Screen dimmer tool:           enabled
    23  * Dimmer backlight:             20%
    24  * Dimmer timeouts:              AC 300  BATT 120
    25  * Screen dpms tool:             enabled
    26  * Dpms timeouts:                AC 900:1200:1800        BATT 600:720:900
    27
    28  (D) [modules.c:42]      Signal module started.
    29  (D) [modules.c:42]      Bus module started.
    30  (D) [modules.c:90]      Trying to start Brightness module as its Bus dependency was loaded...
    31  (D) [modules.c:90]      Trying to start Location module as its Bus dependency was loaded...
    32  (D) [timer.c:31]        Setted timeout of 3s 0ns on fd 6.
    33  (D) [modules.c:42]      Location module started.
    34  (D) [modules.c:90]      Trying to start Upower module as its Bus dependency was loaded...
    35  (D) [modules.c:42]      Upower module started.
    36  (D) [modules.c:90]      Trying to start Brightness module as its Upower dependency was loaded...
    37  (D) [modules.c:90]      Trying to start Dimmer module as its Upower dependency was loaded...
    38  (D) [modules.c:90]      Trying to start Dpms module as its Upower dependency was loaded...
    39  (D) [modules.c:90]      Trying to start Gamma module as its Bus dependency was loaded...
    40  (D) [modules.c:90]      Trying to start Dimmer module as its Bus dependency was loaded...
    41  (D) [modules.c:90]      Trying to start Dpms module as its Bus dependency was loaded...
    42  (E) [bus.c:225] Connection timed out
    43  (D) [modules.c:201]     Location module destroyed.
    44  (D) [modules.c:201]     Upower module destroyed.
    45  (D) [modules.c:201]     Signal module destroyed.
    46  (D) [modules.c:201]     Bus module destroyed.

## 0.16
- [ ] add a CRITICAL_ON_BATT level to customize even more various timeouts/actions
- [ ] dimmer module smooth transitions

## Later
- [ ] add weather support -> New struct for timeouts wuld be something like conf.timeout[enum state][enum weather] where enum weather = { UNWKNOWN, SUNNY, RAINY, CLOUDY } and defaults to 0 obviously -> state.weather = 0; ...or just use something like conf.temp[state.time] that cuts up to 50% at 100% cloudiness
- [ ] improve (amd simplify) modules' handling
