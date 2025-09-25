#pragma once

#include "geometry.h"

#include <stddef.h>

typedef struct {
  const char* classname;
  size_t classname_size;
  Brush* brushes;
  size_t brushes_count;
} MapEntity;

typedef void MapEntityCallback(MapEntity* entity, void* user_data, Arena* temp_arena);

void parse_map(const char* data, MapEntityCallback* entity_callback, void* userdata, Arena* arena);
