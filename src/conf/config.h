#include <log.h>

enum CONFIG { GLOBAL, LOCAL };

int read_config(enum CONFIG file);
int store_config(enum CONFIG file);
