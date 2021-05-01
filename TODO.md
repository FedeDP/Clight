## 5.0

### Generic
- [x] manage sd-login session.Active state property, pausing clight when session is not active

### BACKLIGHT multiple-monitors curves
- [x] Add support for config specific for each monitor
- [x] When present, instead of calling SetAll, Set() clightd method will be called on each curve 
- [x] Clight will call GetAll method to fetch each monitor ID, then for each ID it will call Set using a configured curve (if any) or default curve

- [x] FIX polynomialfit for adjustment curves? -> adjustment curves just tell Clight which backlight level should be set against default curve (eg: default curve says 0.7, adjustment for 'intel_backlight' says 0.73)
- [x] Implement StoreCurveConf
- [x] Fixed StoreCurveConf

### Inhibit
- [x] Add back inhibit_bl conf value
- [x] Add a conf opt to disable inhibit module

### Conf
- [x] Properly dump "disabled" bool in StoreConf for each module

## 5.x
- [ ] Port to libmodule 6.0.0 (?)
- [ ] Add a Dump dbus method (and a DUMP_REQ request) to allow any module to dump their state (module_dump()) to a txt file
