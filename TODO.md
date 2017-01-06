TODO:  

- [ ] *clight* fix algorithm to set screen brightness based on ambient brightness   (MID)
- [ ] *clightd* add support for polkit (is it useful?) (LOW)
- [x] *clight* check for ebusy errors (HIGH)
- [x] *clightd* check for ebusy errors (HIGH)
- [x] *clightd* add a gamma correction tool to clightd bus interface: https://github.com/danielng01/iris-floss/blob/master/iris-floss.c
- [x] *clightd* gracefully manage case where no X display are found
- [x] *clightd* fix get_gamma function
- [x] *clightd* create header with shared includes
- [x] *clightd* update PKGBUILD and README dependencies
- [x] *clightd* make gamma support optional (opt-out)
- [ ] *clight* add geodatabase support + gamma correction based on current time in current timezone in current season (MID)
- [x] *clightd* improve camera.c interface (use global vars and better quit values...)
- [x] *clightd* use same interface as camera.c for gamma.c (ie: passing a pointer to an error variable instead of returning from calls)
