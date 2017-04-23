## High Priority
- [ ] split gamma into 2 fds: 1 is for gamma events(sunrise and sunset), the other for time_range_event and expose conf.gamma_event_range (defaults to 30mins)
- [x] avoid single capture mode to open log and destroy log of running clight instance

## Mid Priority:
- [ ] add an initial setup to ask user to eg: set desired screen backlight level matching current ambient brightness, max brightess captured from webcam (eg: ask him to switch on a torch on webcam lens), and min brightness captured (ask him to cover the webcam). Moreover, set lowest backlight level and ask user if it can see (sometimes at 0 backlight display gets completely dimmed off)
- [x] add a module dependency management system: before calling init, every module will setup its dependencies on other modules. Then, init only module without dep.  
When these modules got started, any of these module should init module that depend on it.  
Ie:  
Brightness require gamma (to know correct timeout to set).  
Gamma require Location.  
Start location; when we receive our first location, start gamma; when gamma finally understands which time of day we are in, start brightness.  
Add inside struct module a struct dep { int refs, enum modules *deps }  
When refs (ie: total number of deps for this module) is 0, init module. This is needed for module with 2+ deps.
Without module dependency manager, a bug is currently happening: if brightness module enters fast_capture mode (ie: brigthness drop > 0.6), but it is started before gamma, then gamma will change its timeout to state.time correct timeout, this invalidating our fast capture.
Brightness module needs to be started after gamma module.

## Low Priority:
- [ ] follow https://github.com/systemd/systemd/issues/5654 (next systemd release?) (need to make a PR upstream) [IN PROGRESS] after this, do a check on SYSTEMD_VERSION >= 234 and use new exposed function in bus_call.
- [ ] add weather support (WIP) New struct for timeouts wuld be something like conf.timeout[enum state][enum weather] where enum weather = { UNWKNOWN, SUNNY, RAINY, CLOUDY } and defaults to 0 obviously -> state.weather = 0; ...or just use something like conf.temp[state.time] that cuts up to 50% at 100% cloudiness 


