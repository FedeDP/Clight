### 3.1
- [x] Properly always emit "Time" signal
- [x] Drop "EVENT" time state. Use a state.in_event boolean and expose it!
- [x] Fix HIGH CPU usage at first start when no_gamma is true
- [x] NoAutoCalib to only stop BACKLIGHT module
- [x] Properly emit Modules' states signals in /org/clight/clight/Modules

- [x] Avoid zeroing log file when trying to start a second clight instance

- [ ] Add wiki page with python script to hook on "Time" clight signal to change theme on kde
- [ ] Updated bus api wiki page


- [ ] Release 3.1!

### 3.2
- [ ] Port to libmodule

### 3.3
- [ ] Add support for config files to give each monitor its own backlight curves. Something like /etc/clight/clight.conf + /etc/clight/mon.d/$MONITOR_SERIAL.conf (where MONITOR_SERIAL can be found through org.clightd.clightd.Backlight.GetAll)
- [ ] If any conf file is found in /etc/clight/mon.d/, avoid calling SetAll, and just call Set on each serial.
