## 3.0

### New Clightd
- [ ] BRIGHTNESS module will add a match on clightd WebcamChanged signal if clightd >= 3.0 is found, and will react to it appropriately
- [ ] Add support for new Clightd ALS interface

### Fixes
- [x] Do not require both --sunrise and --sunset options!
- [x] Do not leave clight if --sunrise or --sunset are wrong, start gamma and location instead
- [x] Brightness module should be a HARD dep for dimmer

### Improvements/Features
- [ ] Add a module pause function that stores current timeout and pauses module + a resume fn to restore it (use it for backlight when going dimmed, and wherever needed)
- [ ] add bus interface methods to change timeouts

## Ideas
- [ ] Use the Time PropertiesChanged signal to change KDE/GNOME theme at sunset/sunrise 
- [ ] GNOME: http://www.fandigital.com/2012/06/change-theme-command-line-gnome.html / https://askubuntu.com/questions/546402/how-to-change-gnome-shell-theme-in-ubuntu-14-10)
- [ ] KDE: https://userbase.kde.org/KDE_Connect/Tutorials/Useful_commands#Change_look_and_feel

- [ ] Let currently used conf be saved to disk through Bus interface, with a "StoreConf" method
