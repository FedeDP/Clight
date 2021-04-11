## 4.6

### Generic
- [ ] Port to libmodule 6.0.0 (?)
- [ ] Add a Dump dbus method (and a DUMP_REQ request) to allow any module to dump their state (module_dump()) to a txt file

### BACKLIGHT multiple-monitors curves
- [ ] Add support for config specific for each monitor, something like:
G2LMTJ006007:
{
    ac_regression_points = [ 0.0, 0.15, 0.29, 0.45, 0.61, 0.74, 0.81, 0.88, 0.93, 0.97, 1.0 ];
    batt_regression_points = [ 0.0, 0.15, 0.23, 0.36, 0.52, 0.59, 0.65, 0.71, 0.75, 0.78, 0.80 ];
}
intel_backlight:
{
    ac_regression_points = [ 0.0, 0.15, 0.29, 0.45, 0.61, 0.74, 0.81, 0.88, 0.93, 0.97, 1.0 ];
    batt_regression_points = [ 0.0, 0.15, 0.23, 0.36, 0.52, 0.59, 0.65, 0.71, 0.75, 0.78, 0.80 ];
}
- [ ] When present, instead of calling SetAll, Set() clightd method will be called on each curve 
- [ ] Clight will call GetAll method to fetch each monitor ID, then for each ID it will call Set using a configured curve (if any) or default curve

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

