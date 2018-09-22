## 3.0
- [ ] BRIGHTNESS module will add a match on clightd WebcamChanged signal if clightd >= 2.4 is found, and will react to it appropriately (avoid requiring clightd >= 2.4 though)
- [x] Do not require both --sunrise and --sunset options!
- [x] Do not leave clight if --sunrise or --sunset are wrong, start gamma and location instead
- [ ] Add support for new Clightd ALS interface
- [ ] Add a module pause function that stores current timeout and pauses module + a resume fn to restore it (use it for backlight when going dimmed, and wherever needed)
- [ ] Xorg module to wait on x11 socket before starting (/tmp/.X11-unix/X0 i guess) or something like that (inotify watcher on /tmp folder waiting for .X11-unix folder to appear?) and drop systemd timer unit
- [ ] add bus methods to change timeouts

## Ideas
- [ ] Use the Time PropertiesChanged signal to change KDE/GNOME theme at sunset/sunrise 
- [ ] GNOME: http://www.fandigital.com/2012/06/change-theme-command-line-gnome.html / https://askubuntu.com/questions/546402/how-to-change-gnome-shell-theme-in-ubuntu-14-10)
- [ ] KDE: https://userbase.kde.org/KDE_Connect/Tutorials/Useful_commands#Change_look_and_feel

- [ ] Real-time reacting to config changes
