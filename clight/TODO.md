TODO:

## Screen brightness algorithm
- [ ] fix algorithm to set screen brightness based on ambient brightness
- [x] less frequent capture during night time (ie 45mins).
- [ ] Very frequent capture at "event" changing (ie: during sunset or sunrise) -> store "next_event" in state struct too (and check it at every CAPTURE timer)
- [ ] use "event" naming for sunset/sunrise everywhere

## Gamma support
- [x] geoclue2 runtime check (if it is missing, print an error and disable gamma support)

## Generic
- [X] add a LOG(..) function that logs every printf (FINISH support! (gamma.c, location.c))
- [ ] follow https://github.com/systemd/systemd/issues/5654 (next systemd release?)
- [x] drop some global vars
- [ ] fix blocking capture: make each capture on its own and set a timer of some ms/ns between each. After conf.num_captures captures, set brightness and set conf.timeout[] timeout.
If single capture mode is ON, just do conf.num_captures captures and leave.
