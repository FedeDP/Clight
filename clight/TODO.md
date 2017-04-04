TODO before release:

## Screen brightness algorithm
- [ ] fix algorithm to set screen brightness based on ambient brightness (low priority | needs real world testing)
- [x] less frequent capture during night time (ie 45mins).
- [ ] Very frequent capture at "event" changing (ie: during sunset or sunrise)  (high priority)
- [x] Fix conf options in case of "EVENT" or "UNKNOWN" state.time

## Geoclue2 support
- [x] geoclue2 runtime check (if it is missing, print an error and disable gamma support)

## Generic
- [X] add a LOG(..) function that logs every printf
- [ ] follow https://github.com/systemd/systemd/issues/5654 (next systemd release?) (low priority | code cosmetic)
- [x] drop some global vars
- [x] more robust error checking
