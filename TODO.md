## 2.2
- [x] Properly implement required signature for LIBSYSTEMD_VERSION < 234 in bus.c call()
- [x] Automatically leave clight if no functional modules is started(eg: no brightness, gamma, dimmer, dpms)
- [x] Fix bug when signature is NULL, it is an undefined behaviour
- [x] Various improvements in modules
- [x] Fix issues on ubuntu 16.04
- [ ] Fix dimmer/backlight issue (probably a clightd issue)

## 2.3
- [ ] Switch to [libmodule](https://github.com/FedeDP/libmodule) to manage modules

## 2.X
- [ ] add a small bus interface to query clight status/set new timeouts/make new fast capture ("-c" switch to follow this new api)
- [ ] Clight single capture log should be logged to same log as main clight (obviously as it will be now redirected to new api (that will just reset brightness timeout to 1ns))


## Later/ideas
- [ ] add a kde/gnome module to set a dark theme on sunrise/sunset (gnome: http://www.fandigital.com/2012/06/change-theme-command-line-gnome.html)
