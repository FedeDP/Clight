#pragma once

#include "commons.h"

enum CONFIG { GLOBAL, LOCAL, CUSTOM };

int read_config(enum CONFIG file, char *config_file);
int store_config(enum CONFIG file);
