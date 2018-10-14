## 3.0

### New Clightd
- [ ] BRIGHTNESS module will add a match on clightd SensorChanged signal and will react to it appropriately (leave pause state when sensor becomes available, enter paused state when sensor is not available)
- [x] BRIGHTNESS module will start paused if no sensors are available; DIMMER module will then stay not-inited until a sensor becomes available (because until first capture is done, we do not knok current_br_pct)
- [x] Add support for new Clightd interface
- [ ] Improve MINIMUM_CLIGHTD_VERSION_MAJ to 3 before release!!!

### Fixes
- [x] Do not require both --sunrise and --sunset options!
- [x] Do not leave clight if --sunrise or --sunset are wrong, start gamma and location instead
- [x] Brightness module should be a HARD dep for dimmer

### Improvements
- [ ] Add a module pause function that stores current timeout and pauses module + a resume fn to restore it (use it for backlight when going dimmed, and wherever needed)
- [ ] Every module has a void* where it can store its internal state data. It is malloc'd and free'd by module interface
- [x] Change default num of captures to 1
- [x] Add a secondary option to desktop file to Inhibit dimmer (https://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#extra-actions)

### Bus API
- [x] add bus interface methods to change timeouts (dimmer, dpms and brightness modules)
- [x] Add a bus interface method to change video interface and backlight sysname while running
- [x] Expose all conf structure through interface in org/clight/clight/Conf
- [a] Add a "StoreConf" method that dumsp current config to user conf
- [x] Remove struct location loc from conf, and put it in state
- [x] FIX: properly account for state.time == EVENT in SetGamma bus call
- [ ] FILL_MATCH_DATA() macro to forcefully use state.userdata instead of relying on casting void *userdata
- [ ] use FILL_MATCH_DATA for exposed configurations too

## Ideas
- [ ] Use the Time PropertiesChanged signal to change KDE/GNOME theme at sunset/sunrise 
- [ ] GNOME: http://www.fandigital.com/2012/06/change-theme-command-line-gnome.html / https://askubuntu.com/questions/546402/how-to-change-gnome-shell-theme-in-ubuntu-14-10)
- [ ] KDE: https://userbase.kde.org/KDE_Connect/Tutorials/Useful_commands#Change_look_and_feel
