#pragma once

#include <almond.h>
#include "arena.h"

typedef struct {
    glm::vec3 normal;
    glm::vec3 anchor;
    glm::vec2 offset;
    glm::vec2 scale;
    float rotation;
    StringView material;
} Plane;

typedef struct {
    Plane* points;
    size_t count;
} Brush;

Plane plane_from_points(glm::vec3 a, glm::vec3 b, glm::vec3 c);
MeshData brush_to_mesh(Brush brush, Arena& arena);
