## Mid Priority:
- [ ] add an initial setup to ask user to eg: set desired screen backlight level matching current ambient brightness, max brightess captured from webcam (eg: ask him to switch on a torch on webcam lens), and min brightness captured (ask him to cover the webcam).

## Low Priority:
- [ ] follow https://github.com/systemd/systemd/issues/5654 (next systemd release?) (need to make a PR upstream) [SENT-PR] after this, do a check on SYSTEMD_VERSION >= 234 and use new exposed function in bus_call.
- [x] upload deb packages after first release
- [x] enum states time UNKNOWN should be 0 index -> ie: enum states { UNKNOWN, DAY, NIGHT, EVENT } -> this way it defaults to UNKNOWN when no gamma support
- [ ] add weather support (WIP) New struct for timeouts wuld be something like conf.timeout[enum state][enum weather] where enum weather = { UNWKNOWN, SUNNY, RAINY, CLOUDY } and defaults to 0 obviously -> state.weather = 0; ...or just use something like conf.temp[state.time] that cuts up to 50% at 100% cloudiness 

## Ideas
Catch active session signals from logind. If current session becomes not active, clight should be paused.  


