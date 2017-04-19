## Mid Priority:
- [ ] add an initial setup to ask user to eg: set desired screen backlight level matching current ambient brightness, max brightess captured from webcam (eg: ask him to switch on a torch on webcam lens), and min brightness captured (ask him to cover the webcam).

## Low Priority:
- [ ] follow https://github.com/systemd/systemd/issues/5654 (next systemd release?) (need to make a PR upstream) [SENT-PR] after this, do a check on SYSTEMD_VERSION >= 234 and use new exposed function in bus_call.
- [x] upload deb packages after first release
- [x] enum states time UNKNOWN should be 0 index -> ie: enum states { UNKNOWN, DAY, NIGHT, EVENT } -> this way it defaults to UNKNOWN when no gamma support

## Ideas
Catch active session signals from logind. If current session becomes not active, clight should be paused.  

Move gamma support into clight (from clightd)?   
PROS:  
* avoid calling xhost
* drop xhost dep 

CONS:  
* gamma support would not be exported in clightd bus interface anymore

Libweather through libcurl?
New struct for timeouts wuld be something like conf.timeout[enum state][enum weather] where enum weather = { UNWKNOWN, SUNNY, RAINY, CLOUDY } and defaults to 0 obviously -> state.weather = 0;
Obviously it cannot be exposed with cmdline options...  
PROS:  
* longer timeouts between captures in sunny days
* shorter timeouts between captures during cloudy/rainy days
* more granular control given to users

CONS:
* i should write it from scratch (well, not really: https://github.com/HalosGhost/shaman/blob/master/src/weather.c)
