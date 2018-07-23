## 2.4
- [x] Properly destroy location module if geoclue could not be inited, even if a cached locations is present
- [x] Fix dimmer

## 2.5
- [ ] Switch to [libmodule](https://github.com/FedeDP/libmodule) to manage modules

## 2.X
- [ ] add a small bus interface to query clight status/set new timeouts/make new fast capture ("-c" switch to follow this new api)
- [ ] Clight single capture log should be logged to same log as main clight (obviously as it will be now redirected to new api (that will just reset brightness timeout to 1ns))


## Later/ideas
- [ ] Properly add a sunrise/sunset signal in clight
- [ ] add a kde/gnome module to set a dark theme on sunrise/sunset

- [ ] GNOME: http://www.fandigital.com/2012/06/change-theme-command-line-gnome.html / https://askubuntu.com/questions/546402/how-to-change-gnome-shell-theme-in-ubuntu-14-10)

- [ ] KDE: https://userbase.kde.org/KDE_Connect/Tutorials/Useful_commands#Change_look_and_feel

- [ ] Add a clight command for kdeconnect to run "clight -c" (is that even possible? -> yes, see .config/kdeconnect/X/kdeconnect_runcommand/config
