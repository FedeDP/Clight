## 2.1

- [x] Dimmer should be available even in Brightness gets disabled (eg: on desktop PCs without webcam)
- [ ] Shorten dimmer timeouts (45 on AC, 20 on batt?)
- [ ] drop sytemd service timer?
- [ ] Use BindsTo instead of require for Clightd in systemd service?
- [ ] add a kde/gnome module to set a dark theme on sunrise/sunset (gnome: http://www.fandigital.com/2012/06/change-theme-command-line-gnome.html)
- [ ] properly add a onBusCallFail callback to bus_args, with a default one like the current check_err function
- [ ] add a small bus interface to query clight status/set new timeouts/make new fast capture ("-c" switch to follow this new api)
