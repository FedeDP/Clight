## 2.1
- [x] Dimmer should be available even in Brightness gets disabled (eg: on desktop PCs without webcam)
- [x] Shorten dimmer timeouts (45 on AC, 20 on batt?)
- [x] use __TIME__ and __DATE__ macros in logger
- [ ] Switch to [libmodule](https://github.com/FedeDP/libmodule) to manage modules

## 2.2
- [ ] add a small bus interface to query clight status/set new timeouts/make new fast capture ("-c" switch to follow this new api)
- [ ] Clight single capture log should be logged to same log as main clight (obviously as it will be now redirected to new api (that will just reset brightness timeout to 1ns))

## Later/ideas
- [ ] add a kde/gnome module to set a dark theme on sunrise/sunset (gnome: http://www.fandigital.com/2012/06/change-theme-command-line-gnome.html)
