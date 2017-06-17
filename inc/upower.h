#include "bus.h"

typedef void (*upower_cb)(int old_state);

void set_upower_self(void);
void add_upower_module_callback(upower_cb callback);
