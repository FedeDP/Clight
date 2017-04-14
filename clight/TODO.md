TODO:

## Mid Priority:
- [ ] fix algorithm to set screen brightness based on ambient brightness (needs real world testing)
- [ ] add an initial setup to ask user to eg: set desired screen backlight level matching current ambient brightness, max brightess captured from webcam (eg: ask him to switch on a torch on webcam lens), and min brightness captured (ask him to cover the webcam).

## Low Priority:
- [ ] follow https://github.com/systemd/systemd/issues/5654 (next systemd release?) (need to make a PR upstream)
- [x] Re-add libconfig support? It will probably be needed for initial setup (to store informatios)
- [ ] update global README with Debian folder/packages -> upload deb packages?
- [x] add a global version flag
- [x] ship default libconfig config file and install it in /etc/default
- [x] disable gamma support if user is on wayland/on console
- [x] avoid 2+ instances of clight running (if conf.single_capture_mode is disabled obviously)
