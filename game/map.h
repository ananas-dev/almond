#pragma once

#include "geometry.h"
#include "string_view.h"

typedef struct {
  StringView classname;
  Brush* brushes;
  size_t brushes_count;
} MapEntity;

typedef void MapEntityCallback(MapEntity* entity, void* user_data, Arena& temp_arena);

void parse_map(const char* data, MapEntityCallback* entity_callback, void* userdata, Arena& arena);
