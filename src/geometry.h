#pragma once

#include "arena.h"
#include <cglm/cglm.h>

typedef struct {
    vec3 vertices[16]; // Edges upper-bound
    size_t count;
} Polygon;

typedef struct {
    Polygon* faces;
    size_t count;
} Polyhedron;

typedef struct {
    vec3* vertices;
    size_t vertices_count;

    ivec3* faces;
    size_t faces_count;
} Mesh;

typedef struct {
    vec3 normal;
    vec3 anchor;
} Plane;

typedef struct {
    Plane* points;
    size_t count;
} Brush;

Plane plane_from_points(vec3 a, vec3 b, vec3 c);
void brush_to_mesh(Brush brush, Arena* arena);
