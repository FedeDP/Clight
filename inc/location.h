#include "bus.h"

typedef void (*location_cb)(void);

void set_location_self(void);
void add_location_module_callback(location_cb cb);
