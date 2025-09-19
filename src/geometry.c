#include "geometry.h"

#include <SDL3/SDL_stdinc.h>

static vec3 unit_cube_vertices[8] = {
    { -0.5f, -0.5f, 0.5f }, //0
    { 0.5f, -0.5f, 0.5f }, //1
    { -0.5f, 0.5f, 0.5f }, //2
    { 0.5f, 0.5f, 0.5f }, //3
    { -0.5f, -0.5f, -0.5f }, //4
    { 0.5f, -0.5f, -0.5f }, //5
    { -0.5f, 0.5f, -0.5f }, //6
    { 0.5f, 0.5f, -0.5f } //7
};

static ivec4 unit_cube_faces[6] = {
    // Top (z = 0.5)
    { 2, 3, 7, 6 },
    // Bottom (z = -0.5)
    { 0, 4, 5, 1 },
    // Left (x = -0.5)
    { 0, 2, 6, 4 },
    // Right (x = 0.5)
    { 1, 5, 7, 3 },
    // Front (y = -0.5)
    { 0, 1, 3, 2 },
    // Back (y = 0.5)
    { 4, 6, 7, 5 }
};


Plane plane_from_points(vec3 a, vec3 b, vec3 c)
{
    Plane plane = { 0 };

    vec3 b_minus_a;
    vec3 c_minus_a;
    glm_vec3_sub(b, a, b_minus_a);
    glm_vec3_sub(c, a, c_minus_a);

    glm_vec3_crossn(b_minus_a, c_minus_a, plane.normal);
    glm_vec3_copy(a, plane.anchor);

    return plane;
}

static bool is_inside_half_plane(Plane plane, vec3 x)
{
    vec3 x_minus_s;
    glm_vec3_sub(x, plane.anchor, x_minus_s);
    return glm_vec3_dot(x_minus_s, plane.normal) <= 0;
}

float edge_plane_intersection(Plane plane, vec3 p, vec3 c)
{
    vec3 p_minus_s;
    vec3 c_minus_p;

    glm_vec3_sub(p, plane.anchor, p_minus_s);
    glm_vec3_sub(c, p, c_minus_p);

    float numerator = glm_vec3_dot(p_minus_s, plane.normal);
    float denominator = glm_vec3_dot(c_minus_p, plane.normal);

    // t = -(p - s) · n / (c - p) · n
    return -numerator / denominator;
}

void brush_to_mesh(Brush brush, Arena* arena)
{
    assert(brush.count + 4 <= 16 && "Too many cuts");

    Polyhedron polyhedra[2] = {0};

    polyhedra[0].faces = PushArrayAligned(arena, Polygon, 6 + brush.count);
    polyhedra[1].faces = PushArrayAligned(arena, Polygon, 6 + brush.count);

    int current_polyhedron = 0;

    // Fill the current polyhedron with a huge cube
    polyhedra[0].count = 6;
    for (int i = 0; i < 6; i++) {
        Polygon* poly = &polyhedra[current_polyhedron].faces[i];

        poly->count = 4;

        for (int j = 0; j < 4; j++) {
            glm_vec3_scale(unit_cube_vertices[unit_cube_faces[i][j]], 8192, poly->vertices[j]);
        }
    }

    for (size_t i = 0; i < brush.count; i++) {
        Plane plane = brush.points[i];

        polyhedra[1 - current_polyhedron].count = 0;

        Polygon *clipped_poly = &polyhedra[1 - current_polyhedron].faces[polyhedra[1 - current_polyhedron].count++];
        clipped_poly->count = 0;

        for (size_t j = 0; j < polyhedra[current_polyhedron].count; j++) {
            Polygon* poly = &polyhedra[current_polyhedron].faces[j];

            Polygon *new_poly = &polyhedra[1 - current_polyhedron].faces[polyhedra[1 - current_polyhedron].count++];
            new_poly->count = 0;

            for (size_t k = 0; k < poly->count; k++) {
                vec3 p;
                vec3 c;
                glm_vec3_copy(poly->vertices[k], p);
                glm_vec3_copy(poly->vertices[(k + 1) % poly->count], c);

                bool is_p_inside = is_inside_half_plane(plane, p);
                bool is_c_inside = is_inside_half_plane(plane, c);

                if (is_p_inside && is_c_inside) {
                    glm_vec3_copy(c, new_poly->vertices[new_poly->count++]);
                } else if (is_p_inside || is_c_inside) {
                    float t = edge_plane_intersection(plane, p, c);
                    vec3 intersection;
                    glm_vec3_lerp(p, c, t, intersection);

                    glm_vec3_copy(intersection, new_poly->vertices[new_poly->count++]);
                    glm_vec3_copy(intersection, clipped_poly->vertices[clipped_poly->count++]);
                }
            }
        }

        current_polyhedron = (current_polyhedron + 1) % 2;
    }

    // TODO: Triangulate and return mesh
}