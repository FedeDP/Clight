## TODO
- [ ] poll socket fd for data by poll instead of waiting on recv (https://stackoverflow.com/questions/2876024/linux-is-there-a-read-or-recv-from-socket-with-timeout)
- [ ] use sd-event library instead of main poll: http://0pointer.net/blog/introducing-sd-event.html

## Later/Ideas
- [ ] subscribe to "interfaceEnabledChanged" signal from clightd (as soon as it is implemented in clightd) and do a capture as soon as interface became enabled (May be disable both dimmer and brightness while interface is disabled)
