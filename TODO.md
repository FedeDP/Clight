## 0.11
### Desktop file (High priority)
- [x] add a desktop file
- [x] add an icon for desktop file
- [x] put clight desktop file in /etc/xdg/autostart (so it will be autostarted), clightc desktop file in /usr/share/applications and UPDATE README
- [x] install icon to correct folder: /usr/share/icons/hicolor/scalable/apps/clight.svg
- [x] fix icon -> real svg not embedded one

### Generic
- [x] get_timeout function to retrieve timerfd remaining timer (easy) -> in GAMMA avoid setting new timeout (based on new enum states time) to N minutes (eg: isntead of setting  new timeout to 45mins during the night, set it to 45mins - elapsed time since last timer); same goes for upower module obviously
- [x] update clight checking if EPERM is returned from bus calls to clightd (it means this clight session is no more active). Do not fail. (easy)
- [x] make event_duration customizable too (easy)

## 0.12
- [ ] Upower battery/ac signals monitor? Ie: add a match on bus. May be more frequent captures if on AC, and less frequent on battery? (easy)

## Later
### Mid Priority:
- [ ] add an initial setup to ask user to eg: set desired screen backlight level matching current ambient brightness, max brightess captured from webcam (eg: ask him to switch on a torch on webcam lens), and min brightness captured (ask him to cover the webcam). Moreover, set lowest backlight level and ask user if it can see (sometimes at 0 backlight display gets completely dimmed off) (mid/Needed?)


### Low Priority:
- [ ] cache latest location retrieved to be taken next time clight starts if geoclue does not give us any location (eg: no/poor internet connection) (easy/Needed?)

### Ideas
- [ ] add weather support -> New struct for timeouts wuld be something like conf.timeout[enum state][enum weather] where enum weather = { UNWKNOWN, SUNNY, RAINY, CLOUDY } and defaults to 0 obviously -> state.weather = 0; ...or just use something like conf.temp[state.time] that cuts up to 50% at 100% cloudiness (mid)
