## 2.3
- [x] Fix bus check_err(): avoid ERROR macro call. It is useless as any error is now caught in code.

## 2.4
- [ ] Switch to [libmodule](https://github.com/FedeDP/libmodule) to manage modules

## 2.X
- [ ] add a small bus interface to query clight status/set new timeouts/make new fast capture ("-c" switch to follow this new api)
- [ ] Clight single capture log should be logged to same log as main clight (obviously as it will be now redirected to new api (that will just reset brightness timeout to 1ns))


## Later/ideas
- [ ] add a kde/gnome module to set a dark theme on sunrise/sunset (gnome: http://www.fandigital.com/2012/06/change-theme-command-line-gnome.html)

- [ ] KDE: https://userbase.kde.org/KDE_Connect/Tutorials/Useful_commands#Change_look_and_feel
- [ ] Add a clight command for kdeconnect to run "clight -c"? (is that even possible -> yes, see .config/kdeconnect/X/kdeconnect_runcommand/config
