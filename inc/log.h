#include <systemd/sd-journal.h>
#include <stdarg.h>
#include <stdio.h>
#include "commons.h"

void _log(FILE *type, const char *format, ...);
void _perror(const char *str);
