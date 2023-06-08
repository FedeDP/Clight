#pragma once

#include "commons.h"

enum CONFIG { GLOBAL, OLD_GLOBAL, LOCAL, CUSTOM };

void init_config_file(enum CONFIG file, char *filename);
int read_config(enum CONFIG file, char *config_file);
int store_config(enum CONFIG file);
