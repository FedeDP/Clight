## 2.1
- [x] Dimmer should be available even in Brightness gets disabled (eg: on desktop PCs without webcam)
- [ ] Shorten dimmer timeouts (45 on AC, 20 on batt?)
- [ ] Use BindsTo instead of require for Clightd in systemd service
- [ ] use __TIME__ and __DATE__ macros in logger

## 2.2
- [ ] Switch to [libmodule](https://github.com/FedeDP/libmodule) to manage modules

## 2.3
- [ ] add a small bus interface to query clight status/set new timeouts/make new fast capture ("-c" switch to follow this new api)
- [ ] Clight single capture log should be logged to same log as main clight

## Later/ideas
- [ ] add a kde/gnome module to set a dark theme on sunrise/sunset (gnome: http://www.fandigital.com/2012/06/change-theme-command-line-gnome.html)
