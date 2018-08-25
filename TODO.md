## 2.5
- [x] Fix geoclue2 issues
- [x] Install license file in /usr/share/licenses/clight/
- [x] Drop support for /etc/xdg/autostart, and tell user to issue a "systemctl --user enable clight.timer" instead. This way even not-xdg-compliant DE will work out of the box
- [ ] Drop XORG module and let all modules check if they can be enabled in check() function

## 2.6
- [ ] add a small bus interface to query clight status/set new timeouts/make new fast capture ("-c" switch to follow this new api)
- [ ] Clight single capture log should be logged to same log as main clight (obviously as it will be now redirected to new api (that will just reset brightness timeout to 1ns))

## 2.X
- [ ] Switch to [libmodule](https://github.com/FedeDP/libmodule) to manage modules

## Later/ideas
- [ ] Properly add a sunrise/sunset signal in clight
- [ ] add a kde/gnome module to set a dark theme on sunrise/sunset

- [ ] GNOME: http://www.fandigital.com/2012/06/change-theme-command-line-gnome.html / https://askubuntu.com/questions/546402/how-to-change-gnome-shell-theme-in-ubuntu-14-10)

- [ ] KDE: https://userbase.kde.org/KDE_Connect/Tutorials/Useful_commands#Change_look_and_feel

- [ ] Add a clight command for kdeconnect to run "clight -c" (is that even possible? -> yes, see .config/kdeconnect/X/kdeconnect_runcommand/config
