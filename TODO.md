## 1.5
- [x] location_callback for weather module
- [x] each location_callback should check if distance is > N meters/kilometers to avoid useless calls.
- [x] Location module should expose a get_distance(old pos, new pos) function
- [x] add a data struct location { double lat, double lon } and use that everywhere
- [x] Check latitude/longitude values after loading them from opts/config

## Later/Ideas
- [ ] subscribe to "interfaceEnabledChanged" signal from clightd (as soon as it is implemented in clightd) and do a capture as soon as interface became enabled (May be disable both dimmer and brightness while interface is disabled)
