#include <systemd/sd-bus.h>
#include <stdarg.h>

#include "commons.h"

void init_bus(void);
void bus_call(void *userptr, const char *userptr_type, const char *method, const char *signature, ...);
void destroy_bus(void);
