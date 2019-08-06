#pragma once

#include <bus.h>

#define EMIT_P(topic, ptr)  emit_prop(topic, self(), ptr, sizeof(*ptr))

int emit_prop(const char *topic, const self_t *self, const void *ptr, const int size);
