#pragma once

#include <almond.h>
#include "arena.h"

typedef struct {
    Vector3 normal;
    Vector3 anchor;
    Vector2 offset;
    Vector2 scale;
    float rotation;
    StringView material;
} Plane;

typedef struct {
    Plane* points;
    size_t count;
} Brush;

Plane plane_from_points(Vector3 a, Vector3 b, Vector3 c);
MeshData brush_to_mesh(Brush brush, Arena* arena);
