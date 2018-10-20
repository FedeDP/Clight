## 3.0

### New Clightd
- [x] BRIGHTNESS module will add a match on clightd SensorChanged signal and will react to it appropriately (leave pause state when sensor becomes available, enter paused state when sensor is not available)
- [x] BRIGHTNESS module will start paused if no sensors are available; DIMMER module will then stay not-inited until a sensor becomes available (because until first capture is done, we do not knok current_br_pct)
- [x] Add support for new Clightd interface
- [ ] Dimmer too should be paused when brightness is paused

### Fixes
- [x] Do not require both --sunrise and --sunset options!
- [x] Do not leave clight if --sunrise or --sunset are wrong, start gamma and location instead
- [x] Brightness module should be a HARD dep for dimmer

### Improvements
- [x] Change default num of captures to 1
- [x] Add a secondary option to desktop file to Inhibit dimmer (https://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#extra-actions)

### Bus API
- [x] add bus interface methods to change timeouts (dimmer, dpms and brightness modules)
- [x] Add a bus interface method to change video interface and backlight sysname while running
- [x] Expose all conf structure through interface in org/clight/clight/Conf
- [x] Add a "StoreConf" method that dumps current config to user conf
- [x] Remove struct location loc from conf, and put it in state
- [x] FIX: properly account for state.time == EVENT in SetGamma bus call
- [x] FILL_MATCH_DATA() macro to forcefully use state.userdata instead of relying on casting void *userdata
- [x] use FILL_MATCH_DATA for exposed configurations too
- [x] Remove ApplyGamma and automatically set new temperature when DailyTemp/NightTemp are changed
- [x] Rename "StoreConf" to only "Store" as it is already under /org/clight/clight/Conf path

### RELEASE

- [ ] Update API doc
- [ ] Improve MINIMUM_CLIGHTD_VERSION_MAJ to 3

## Ideas
- [ ] Use the Time PropertiesChanged signal to change KDE/GNOME theme at sunset/sunrise 
- [ ] GNOME: http://www.fandigital.com/2012/06/change-theme-command-line-gnome.html / https://askubuntu.com/questions/546402/how-to-change-gnome-shell-theme-in-ubuntu-14-10)
- [ ] KDE: https://userbase.kde.org/KDE_Connect/Tutorials/Useful_commands#Change_look_and_feel
