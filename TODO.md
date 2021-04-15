## 4.6

### Generic
- [ ] Port to libmodule 6.0.0 (?)
- [ ] Add a Dump dbus method (and a DUMP_REQ request) to allow any module to dump their state (module_dump()) to a txt file

### BACKLIGHT multiple-monitors curves
- [x] Add support for config specific for each monitor
- [x] When present, instead of calling SetAll, Set() clightd method will be called on each curve 
- [x] Clight will call GetAll method to fetch each monitor ID, then for each ID it will call Set using a configured curve (if any) or default curve

- [x] FIX polynomialfit for adjustment curves? -> adjustment curves just tell Clight which backlight level should be set against default curve (eg: default curve says 0.7, adjustment for 'intel_backlight' says 0.73)
- [x] Implement StoreCurveConf

### Bus
- [ ] Expose BUS_REQ to make dbus call from custom modules -> 
CALL: sd_bus_message_appendv(m, signature, args) + sd_bus_message_readv(m, signature, args);
SET_PROP: sd_bus_set_propertyv
GET_PROP; sd_bus_get_property() + sd_bus_message_readv()
typedef struct {
    enum bus_call_type type;
    va_list write_args;
    va_list read_args;
} bus_req;

