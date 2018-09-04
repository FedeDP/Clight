## 2.5
- [x] Fix geoclue2 issues
- [x] Install license file in /usr/share/licenses/clight/
- [x] Drop support for /etc/xdg/autostart, and tell user to issue a "systemctl --user enable clight.timer" instead. This way even not-xdg-compliant DE will work out of the box
- [x] add a small bus interface to query clight status/set new timeouts/make new fast capture ("-c" switch to follow this new api)
- [x] Clight single capture log should be logged to same log as main clight (obviously as it will be now redirected to new api (that will just reset brightness timeout to 1ns))
- [x] Clightd module should depend on bus
- [x] Automatically set module self at startup
- [x] Drop unused code
- [x] Require c99 as no more c11 is used
- [x] Better code organization and repo architecture

- [x] Improve bus interface with more methods (setgamma + inhibit (presentation mode))
- [x] rename INITED state to RUNNING state
- [x] Dropped state.fast_recapture leftover references
- [ ] Add API reference for bus interface
- [ ] Add gh wiki pages
- [ ] Add completion script for bash
- [x] Drop lock files: when INTERFACE module requests a bus name, request will fail if another daemon is already registered

## Later/ideas
- [ ] Add a bus signal "TimeChanged" on sunrise/sunset that returns an enum time { SUNRISE, SUNSET }
- [ ] Use the TimeChanged signal to change KDE/GNOME theme at sunset/sunrise 

- [ ] GNOME: http://www.fandigital.com/2012/06/change-theme-command-line-gnome.html / https://askubuntu.com/questions/546402/how-to-change-gnome-shell-theme-in-ubuntu-14-10)

- [ ] KDE: https://userbase.kde.org/KDE_Connect/Tutorials/Useful_commands#Change_look_and_feel
