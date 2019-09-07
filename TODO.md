### 4.0
- [x] Fix build when LIBSYSTEMD_VERSION is a float (?), #70

#### Backlight
- [x] FIX issue with call() variadic argument for keyboard backlight #94

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

#### Increment points size for backlight curve
- [x] Fix #95

#### DPMS
- [x] Dpms should be a special case of screen dimming, ie: a Clightd.Idle client that waits on a timeout, and when IDLE signal is sent, it should switch off screen.
- [x] Modules should hook on "Dpms" prop and stop working? Just like "Dimmed" property!
- [x] Something like state.display_state -> DIMMED = 1, DPMS = 2 , BOTH = 1 | 2. Callbacks will only check if state.display_state is true/false and act accordingly
- [x] Drop xorg module
- [x] Fix idle_set_timeout() -> we should not start a client when setting timeout if we are in pm_inhibited or timeout is < 0
- [x] Properly validate client in idler.c interface
- [x] Bump clightd required version to 4.0
- [x] Update doc

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
- [x] Properly delete geoclue client when leaving

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
- [x] FIx rpm support

#### Libmodule
- [x] Require libmodule 5.0.0 in CMakeLists
- [x] Port modules to libmodule 5.0.0
- [x] Port main to use libmodule
- [x] Port all "no-X" options to a conf.no_X
- [x] Drop modules.{c/h}
- [x] Restore emit_prop that will publish a message with "self" and will emit dbus signal
- [x] Drop org.clight.clight.Module interface
- [x] Fix Assertion 'close_nointr(fd) != -EBADF' failed at ../systemd-stable/src/basic/fd-util.c:71, function safe_close(). Aborting.
Annullato (core dump creato)
- [x] GAMMA: emit "Sunrise" and "Sunset" props even on error!
- [x] Add back module name in various logs
- [x] FIX: gamma when fixed position is set from conf
- [x] FIX: gamma when fixed sunrsie/sunset times are set from conf
- [x] FIX: upower topic
- [x] conf.sunrise/sunset writable from bus api
- [x] move check outside of do_capture in backlight
- [x] sd_bus_process both sysbus and userbus
- [x] Let INTERFACE own sd_bus *userbus
- [x] Add new conf options readonly
- [x] Drop emit_prop and just let INTERFACE module subscribe to desired topics, and sd_bus_emit_properties_changed() on topic
- [x] FIX: in INTERFACE set_event(), we should publish a message for GAMMA module to re-compute state.events
- [x] FIX: in geoclue on_new_location, avoid overriding conf.location if it has been set by interface API
- [x] FIX: fix memleaks!
- [x] FIX: interface Version and ClightdVersion properties
- [x] FIX: state.ac_state should be -1 on start and various modules should wait on it (same as state.time) -> BACKLIGHT, DIMMER, DPMS
- [x] Cleanup includes
- [x] Modules should set new state/conf themselves from message values. This way when a custom modules send a message, everything is managed internally. 
Eg: on new location we should send a message with new lat and lon and LOCATION module should store them in state.current_loc.
-> split eg: LOCATION_UPD and LOCATION_UPD_REQ; request has to have a "new" field with new value.
-> LOCATION, on new geocluelocation, will m_tell(self(), LOCATION_UPD_REQ) and then update state.current_loc and publish LOCATION_UPDATED
- [x] Add a new "public.h" header to hide everything except PubSub and logs (but not ERROR)
- [x] Test on wayland/tty
- [x] Let user call BL_REQ or KBD_BL_REQ too!
- [x] Dimmer should use BL_REQ and depend on BACKLIGHT
- [x] Drop backlight.h
- [x] smooth options for backlight/gamma should be settable by BL_REQ/TEMP_REQ message

#### Support user supplied modules runtime loading
- [x] Load modules from XDG_DATA_HOME/clight/modules.d/
- [x] Supply a skeleton module to be used as example
- [x] Support global user modules (/usr/share/clight/modules.d ?)
- [x] Install commons.h in /usr/include/clight/
- [x] move topics in commons.h
- [x] Move log.h functions too, so users can log into clight's log
- [x] Validate user message request
- [x] Use a generic macro to validate requests!
- [x] Add SUNRISE_REQ and SUNSET_REQ messages
- [x] Compile time assert correctness of enum mod_msg_type values in DECLARE_MSG and M_SUB calls
- [x] Compile time assert correctness of topics

#### Screen compensation support (#84)
- [x] Expose through dbus API new conf options and react if they change
- [x] Expose through dbus API new state value (screen_comp)
- [x] Fix crash!
- [x] Add to log!
- [x] Properly reset screen_br array values when entering a "screen paused" state (eg: on battery), and restart from scratch filling it.

#### ScreenSaver (#76)
- [x] Implement org.freedesktop.ScreenSaver bus API (Inhibit/UnInhibit methods) 
- [x] If org.freedesktop.ScreenSaver is already present, avoid failing
- [x] Redirect old Inhibit method to new internal api. Keep it available where org.freedesktop.ScreenSaver is already took by DE
- [x] Eventually drop power management api/configs ... (?)
- [x] Drop enum pm_states { PM_OFF, PM_ON, PM_FORCED_ON }
- [x] Drop no_inhibit

#### Inhibition (#81)
- [x] New conf to inhibit automatic calibration too (together with dimmer) on Screensaver inhibition enabled inhibit_autocalib = true
- [x] Expose through interface

#### Generic
- [x] Update conf file/clight autocompletion script/interface.c with new options
- [x] Add issue template
- [x] Reset conf.event_duration default value (changed for testing)
- [x] Update wiki with new features
- [x] Add "pause calib" and "resume calib" actions to desktop file
- [x] Bump to 4.0
- [x] Avoid fill_match_data with old value whenever possible (only when value really changed), and remove useless checks (eg: src/modules/dpms.c:    if (!!old_pm_state != !!state.pm_inhibited) ...)
- [x] Add "--expect-reply=false" to desktop file actions
- [x] Log bus calls only in verbose mode
- [x] Fix clightd going mad if clight is stopped (small bug in libmodule)
- [x] Automatically detect CLightd features and disable corresponding clight modules
- [x] Force eg: conf.no_gamma when not on X? This way, it would be accounted when checking functional running modules...
- [x] Fix bug in reset_timer with <0 old values.
- [x] Rework clight conf file
- [x] Fix bug in idler.c: pass correct timeout in idle_client_start()
- [x] Fix validations not working
- [x] Constify message_t -> type
- [x] Fix display_state validations and messages...
- [x] Fix DPMS/Dimmer: they'll both send a DISPLAY_ON request to DISPLAY when leaving idle. Last request to arrive will fail to validate as we are already DISPLAY_ON.
- [x] Expose a Load/Unload functions from API interface to load new custom modules
- [x] Add support for new Capture method param in Clightd
- [x] Fix 20s timeout on start? Geoclue is super-slow...PR was merged and will be available with next release!: https://gitlab.freedesktop.org/geoclue/geoclue/merge_requests/50

#### DOC/WIKI updates
- [x] Drop API wiki page about Modules bus interface
- [x] Updated API ("CurrentTemp" -> "Temp" ecc ecc)
- [x] Provide a working module in wiki pages to switch user theme on SUNRISE/SUNSET events
- [x] Provide a working module in wiki pages to set a certain backlight level when entering inhibit state
- [x] Update API wiki for screen compensation (#84)
- [x] Update API wiki for inhibit_autocalib (#81)
- [x] Update API wiki dropping NoInhibit (#76)
- [x] Update API wiki with new Load/Unload functions
- [ ] Update API wiki with new SensorSettings writable property

### 4.X

#### BACKLIGHT multiple-monitors curves
- [ ] Add support for config files to give each monitor its own backlight curves. Something like /etc/clight/clight.conf + /etc/clight/mon.d/$MONITOR_SERIAL.conf (where MONITOR_SERIAL can be found through org.clightd.clightd.Backlight.GetAll)
- [ ] If any conf file is found in /etc/clight/mon.d/, avoid calling SetAll, and just call Set on each serial.
