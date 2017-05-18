## 0.14
- [ ] add a backlight dimmer module (http://stackoverflow.com/questions/15472585/track-keyboard-and-mouse-events-in-c-on-linux); i'd use inotify on /dev/input/*; at every notification from it, it means user has moved mouse/used keyboard. In conjunction with a timerfd of conf.dimmer_timeout. When conf.dimmer_timeout is reached, dim screen and at next inotify notification leave dimmed state.
sudo inotifywait -t 5 -e access -m /dev/input/* . It seems inotify requires root access. Put this code in clightd: clightd will send a signal when conf.dimmer_timeout is reached.
-> clight client calls a method like "setdimmer_timeout" passing a timeout, on clightd.
-> for each client timeout, clightd creates a  { timeout, destination } map
-> clight client then sets a match on "dimmed" signal from clightd (with a boolean "dimmed = true/false")
-> clightd adds an inotify watch on /dev/input/* 
-> after timeout has passed for any client, it send "dimmed" signal to that client, using a sd_bus_message_new_signal passing in a sd_bus_message *m that already has correct clight client destination.
-> as soon as a new event is received, if we were in dimmed state, send a dimmed = false signal.

## Later
- [ ] add weather support -> New struct for timeouts wuld be something like conf.timeout[enum state][enum weather] where enum weather = { UNWKNOWN, SUNNY, RAINY, CLOUDY } and defaults to 0 obviously -> state.weather = 0; ...or just use something like conf.temp[state.time] that cuts up to 50% at 100% cloudiness (mid)
