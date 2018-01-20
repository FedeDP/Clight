## 2.0
- [x] location_callback for weather module
- [x] each location_callback should check if distance is > N meters/kilometers to avoid useless calls.
- [x] Location module should expose a get_distance(old pos, new pos) function
- [x] add a data struct location { double lat, double lon } and use that everywhere
- [x] Check latitude/longitude values after loading them from opts/config
- [x] update to new 1.6 clightd interface
- [x] drop isinterface_enabled calls
- [x] fix dimmer with set_backlight_level with percentage
- [x] EXIT_FAILURE even when state.quit > ERR_QUIT (should never happen)
- [x] location callback should check if new position is at least LOC_DISTANCE_THRS distant from last pos
- [x] make clight work on desktop pc with webcam too (obviously through ddcutil)? -> add a setbrightnesspct_all() method to clightd that will set a pct for internal + external monitors
- [x] add a math_utils.c source file with all math functions (eg get_distance, clamp...)?
- [x] make backlight level change smoothly (#20)
- [x] drop weather support
- [ ] update readme
- [ ] TEST 
- [ ] NEW RELEASE!

## 2.1
- [ ] add a kde/gnome module to set a dark theme on sunrise/sunset (gnome: http://www.fandigital.com/2012/06/change-theme-command-line-gnome.html)
