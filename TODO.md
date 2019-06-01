### 4.0
- [x] Fix build when LIBSYSTEMD_VERSION is a float (?), #70

#### Dimmer
- [x] Different timeout for dimmer leaving-dimmed fading transition
- [x] No_smooth_dimmer_transition for enter/leave transitions
- [x] dimmer does not require Xorg anymore. Update to new Clight API.
- [x] Update doc
- [x] require clightd 3.5

#### Conf-file option
- [x] New cmd line option to specify a conf file to be parsed (instead of default $HOME/.conf/clight.conf)
- [x] Fix small conf reading bug

#### Long gamma transitions
- [x] Add a long_gamma_transition option, to change gamma in a way similar to redshift (very long fading trnasition)

#### Ambient-brightness-based gamma
- [x] Fix #61

#### DPMS
- [x] Dpms should be a special case of screen dimming, ie: a Clightd.Idle client that waits on a timeout, and when IDLE signal is sent, it should switch off screen.
- [x] Modules should hook on "Dpms" prop and stop working? Just like "Dimmed" property!
- [x] Something like state.display_state -> DIMMED = 1, DPMS = 2 , BOTH = 1 | 2. Callbacks will only check if state.display_state is true/false and act accordingly
- [x] Drop xorg module
- [x] Fix idle_set_timeout() -> we should not start a client when setting timeout if we are in pm_inhibited or timeout is < 0
- [x] Properly validate client in idler.c interface
- [x] Bump clightd required version to 4.0
- [ ] Update doc

#### Gamma fixes
- [x] Double check state.time + state.in_event usage in BACKLIGHT and user scripts
- [x] Test BACKLIGHT timeouts with verbose option
- [x] FIX: next_event should point to next event as soon as current event is elapsed (eg: SUNRISE 7:00, at 6:59 next_event -> SUNRISE, at 7:00 next_event -> SUNSET)
- [x] FIX gamma long transitions for long suspend cycles (eg: put laptop to sleep during NIGHT and awake it during DAY)
- [x] fix #78

#### Location fixes
- [x] 0.0:0.0 is actually a good location. Avoid using these values to filter away unsetted location
- [x] fix: ->  WARN("Wrong longitude value. Resetting default value.\n");
- [x] Drop get_distance method, directly use Geoclue2 threshold. "TimeThreshold" was needed too: "When TimeThreshold is zero, it always emits the signal."
- [x] Only cache location when leaving if any geoclue2 client was initialized

#### Bus api
- [x] Expose CurrentKbdPct through bus api
- [x] Expose CurrentTemp through bus api
- [x] "NoAutoCalib", "NoKbdCalib", "AmbientGamma" should be writable bus properties (Fixes #50 -> NoAutoCalib)
- [x] Check if module is running before calling its prop callbacks
- [x] Update wiki with new bus api!!

#### Inhibition
- [x] state.pm_inhibited like DisplayState!!!
- [x] Expose state.pm_inhibited

#### Cpack
- [x] Add Cpack support to cmake
- [x] Fix deb support

#### Generic
- [x] Update conf file/clight autocompletion script/interface.c with new options
- [x] Add issue template
- [x] Reset conf.event_duration default value (changed for testing)
- [x] Update wiki with new features
- [x] Add "pause calib" and "resume calib" actions to desktop file
- [x] Bump to 4.0
- [ ] Avoid fill_match_data with old value whenever possible (only when value really changed), and remove useless checks (eg: src/modules/dpms.c:    if (!!old_pm_state != !!state.pm_inhibited) ...)

### 4.1

#### Inhibition (#81)
- [ ] New conf to inhibit automatic calibration too (together with dimmer) -> inhibit_dimmer = true; inhibit_autocalib = true (if both false -> disable inhibit module)
- [ ] New conf to set a certain backlight level when entering inhibit state -> inhibit_backlight = {AC, BATT}. This implicitly will enable inhibit_autocalib=true
- [ ] If inhibit_backlight -> when leaving inhibit state, do a quick capture

#### ScreenSaver (#76)
- [ ] Implement org.freedesktop.ScreenSaver bus API (Inhibit/UnInhibit methods)
- [ ] If org.freedesktop.ScreenSaver is already present, avoid failing
- [ ] Eventually drop power management api/configs ... (?)

### 4.2

#### BACKLIGHT multiple-monitors curves
- [ ] Add support for config files to give each monitor its own backlight curves. Something like /etc/clight/clight.conf + /etc/clight/mon.d/$MONITOR_SERIAL.conf (where MONITOR_SERIAL can be found through org.clightd.clightd.Backlight.GetAll)
- [ ] If any conf file is found in /etc/clight/mon.d/, avoid calling SetAll, and just call Set on each serial.

### 4.X
- [ ] Port to libmodule
