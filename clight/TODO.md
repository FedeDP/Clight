TODO:

## Screen brightness algorithm
- [ ] fix algorithm to set screen brightness based on ambient brightness   (MID)
- [x] less frequent capture during night time (ie 45mins).
- [ ] Very frequent capture at "event" changing (ie: during sunset or sunrise)

## Gamma support
- [x] add gamma correction support
- [x] smooth gamma transition (mid)
- [x] add opt for latitude and longitude
- [x] improve gamma transitioning (see FIXME) (HIGH)
- [x] add geoclue dbus api support (HIGH PRIORITY)
- [x] fix initial wrong bus_process calls
- [ ] geoclue2 runtime check (if it is missing, print an error and disable gamma support)
- [x] add a struct in bus.h to create a {service, interface, method} triplet

## Generic
- [ ] add a LOG(..) function that logs every printf (mid)
- [ ] follow https://github.com/systemd/systemd/issues/5654 (next systemd release?)
- [x] add doc
