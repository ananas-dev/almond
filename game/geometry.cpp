#include "geometry.h"

#include "../src/logger.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define DISTEPSILON 1e-6f
#define GRID_SIZE 1e-2f

struct TexInfo {
    glm::vec2 offset;
    glm::vec2 scale;
    float rotation;
};

struct Polygon {
    glm::vec3 vertices[32]; // Edges upper-bound
    size_t count;
    TexInfo tex_info;
    glm::vec3 normal;
};

typedef struct {
    Polygon* faces;
    size_t count;
} Polyhedron;

static glm::vec3 unit_cube_vertices[8] = {
    { -0.5f, -0.5f, 0.5f }, // 0
    { 0.5f, -0.5f, 0.5f }, // 1
    { -0.5f, 0.5f, 0.5f }, // 2
    { 0.5f, 0.5f, 0.5f }, // 3
    { -0.5f, -0.5f, -0.5f }, // 4
    { 0.5f, -0.5f, -0.5f }, // 5
    { -0.5f, 0.5f, -0.5f }, // 6
    { 0.5f, 0.5f, -0.5f } // 7
};

static uint16_t unit_cube_faces[6][4] = {
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

static glm::vec3 unit_cube_normals[6] = {
    { 0.0f, 0.0f, 1.0f }, // Top (z = 0.5)
    { 0.0f, 0.0f, -1.0f }, // Bottom (z = -0.5)
    { -1.0f, 0.0f, 0.0f }, // Left (x = -0.5)
    { 1.0f, 0.0f, 0.0f }, // Right (x = 0.5)
    { 0.0f, -1.0f, 0.0f }, // Front (y = -0.5)
    { 0.0f, 1.0f, 0.0f } // Back (y = 0.5)
};

Polygon* push_polygon(Polyhedron* polyhedron)
{
    return &polyhedron->faces[polyhedron->count++];
}

static glm::vec3 snap_to_grid(glm::vec3 vertex, float grid_size)
{
    return {
        roundf(vertex.x / grid_size) * grid_size,
        roundf(vertex.y / grid_size) * grid_size,
        roundf(vertex.z / grid_size) * grid_size
    };
}

static glm::vec3 base_axis[6][3] = {
    { { 0, 0, 1 }, { 1, 0, 0 }, { 0, -1, 0 } }, // floor
    { { 0, 0, -1 }, { 1, 0, 0 }, { 0, -1, 0 } }, // ceiling
    { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, -1 } }, // west wall
    { { -1, 0, 0 }, { 0, 1, 0 }, { 0, 0, -1 } }, // east wall
    { { 0, 1, 0 }, { 1, 0, 0 }, { 0, 0, -1 } }, // south wall
    { { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, -1 } } // north wall
};

static void calculate_rotated_uv(
    glm::vec3 normal,
    glm::vec3 u_axis_in,
    glm::vec3 v_axis_in,
    float xscale,
    float yscale,
    float rotation_deg,
    glm::vec3* u_axis_out,
    glm::vec3* v_axis_out)
{
    glm::vec3 scaled_u = u_axis_in * 1.0f / xscale;
    glm::vec3 scaled_v = v_axis_in * 1.0f / yscale;

    glm::vec3 rotation_axis;
    glm::vec3 abs_n = glm::abs(normal);
    if (abs_n.x > abs_n.y && abs_n.x > abs_n.z)
        rotation_axis = glm::vec3(1, 0, 0);
    else if (abs_n.y > abs_n.z)
        rotation_axis = glm::vec3(0, 1, 0);
    else
        rotation_axis = glm::vec3(0, 0, 1);

    if (fabsf(rotation_deg) > 0.001f) {
        glm::mat3 rot = glm::mat3_cast(glm::angleAxis(glm::radians(rotation_deg), rotation_axis));
        *u_axis_out = rot * scaled_u;
        *v_axis_out = rot * scaled_v;
    } else {
        *u_axis_out = scaled_u;
        *v_axis_out = scaled_v;
    }
}

static glm::vec2 calculate_uv(
    glm::vec3 vertex,
    float xshift, float yshift,
    float tex_width, float tex_height,
    glm::vec3 u_axis, glm::vec3 v_axis)
{
    float u = glm::dot(vertex, u_axis) + xshift;
    float v = glm::dot(vertex, v_axis) + yshift;
    u /= tex_width;
    v /= tex_height;
    return {u, v};
}

// O(n)
static int find_or_insert_vertex(MeshData* mesh, glm::vec3 position, TexInfo tex_info, glm::vec3 normal)
{
    glm::vec3 snapped_position = snap_to_grid(position, GRID_SIZE);

    // for (size_t i = 0; i < mesh->vertices_count; i++) {
    //     if (vector3_eq(mesh->vertices[i].position, snapped_position, DISTEPSILON)) {
    //         return (int)i;
    //     }
    // }

    int index = (int)mesh->vertices_count;

    // Find best axis
    float best_dot = -1.0f;
    int bestaxis = 0;
    for (int i = 0; i < 6; i++) {
        float dot = glm::dot(normal, base_axis[i][0]);
        if (dot > best_dot) {
            best_dot = dot;
            bestaxis = i;
        }
    }

    glm::vec3 u_axis = base_axis[bestaxis][1];
    glm::vec3 v_axis = base_axis[bestaxis][2];

    glm::vec3 rotated_u, rotated_v;
    calculate_rotated_uv(
        normal,
        u_axis, v_axis,
        tex_info.scale.x, tex_info.scale.y,
        tex_info.rotation,
        &rotated_u, &rotated_v);

    glm::vec2 uv = calculate_uv(
        snapped_position,
        tex_info.offset.x, tex_info.offset.y,
        32.0f, 32.0f,
        rotated_u, rotated_v);

    mesh->vertices[index].position = snapped_position;
    mesh->vertices[index].texcoords = uv;
    mesh->vertices_count++;

    return index;
}

Plane plane_from_points(glm::vec3 a, glm::vec3 b, glm::vec3 c)
{
    Plane plane = {};

    plane.normal = glm::normalize(glm::cross(b - a, c - a));
    plane.anchor = a;

    return plane;
}

static bool is_inside_half_plane(Plane plane, glm::vec3 x)
{
    return glm::dot(x - plane.anchor, plane.normal) >= 0;
}

glm::vec3 edge_plane_intersection(Plane plane, glm::vec3 p, glm::vec3 c)
{
    glm::vec3 ray_dir = c - p;
    glm::vec3 prev_to_anchor = plane.anchor - p;

    float numerator = glm::dot(prev_to_anchor, plane.normal);
    float denominator = glm::dot(ray_dir, plane.normal);

    glm::vec3 intersection;
    if (fabsf(denominator) > DISTEPSILON) {
        float t = numerator / denominator;
        intersection = p + ray_dir * t;
    } else {
        printf("Malformed brush");
        return {};
    }

    return snap_to_grid(intersection, GRID_SIZE);
}

typedef struct {
    glm::vec3 center;
    glm::vec3 normal;
} ConvexPolygonCompareUserData;

static ConvexPolygonCompareUserData convex_polygon_compare_user_data;

int convex_polygon_compare(const void* a, const void* b)
{
    ConvexPolygonCompareUserData* data = &convex_polygon_compare_user_data;
    auto* A = static_cast<const glm::vec3*>(a);
    auto* B = static_cast<const glm::vec3*>(b);

    glm::vec3 vec_a = *A - data->center;
    glm::vec3 vec_b = *B - data->center;

    glm::vec3 proj_a = glm::normalize(vec_a - data->normal * glm::dot(vec_a, data->normal));
    glm::vec3 proj_b = glm::normalize(vec_b - data->normal * glm::dot(vec_b, data->normal));

    // Calculate angles using atan2
    // We need a consistent reference frame on the plane
    glm::vec3 reference;

    // Create reference vector perpendicular to normal
    if (fabsf(data->normal.x) < 0.9f) {
        reference = glm::cross(data->normal, glm::vec3(1, 0, 0));
    } else {
        reference = glm::cross(data->normal, glm::vec3(0, 1, 0));
    }
    reference = glm::normalize(reference);

    // Create tangent vector (completes the 2D coordinate system)
    glm::vec3 tangent = glm::cross(data->normal, reference);

    // Get 2D coordinates on the plane
    float angle_a = atan2f(glm::dot(proj_a, tangent), glm::dot(proj_a, reference));
    float angle_b = atan2f(glm::dot(proj_b, tangent), glm::dot(proj_b, reference));

    if (angle_a < angle_b)
        return -1;
    if (angle_a > angle_b)
        return 1;

    return 0;
}

MeshData brush_to_mesh(Brush brush, Arena& arena)
{
    Polyhedron polyhedra[2] = {};

    polyhedra[0].faces = arena.PushArray<Polygon>(6 + brush.count);
    polyhedra[1].faces = arena.PushArray<Polygon>(6 + brush.count);

    int current_polyhedron = 0;

    // Fill the current polyhedron with a huge cube
    polyhedra[0].count = 6;
    for (int i = 0; i < 6; i++) {
        Polygon* poly = &polyhedra[current_polyhedron].faces[i];

        // poly->tex_info;
        poly->normal = unit_cube_normals[i];

        poly->count = 4;

        for (int j = 0; j < 4; j++) {
            poly->vertices[j] = snap_to_grid(unit_cube_vertices[unit_cube_faces[i][j]] * 8192.0f, GRID_SIZE);
        }
    }

    for (size_t i = 0; i < brush.count; i++) {
        Plane plane = brush.points[i];

        polyhedra[1 - current_polyhedron].count = 0;

        Polygon clipped_poly = {};

        for (size_t j = 0; j < polyhedra[current_polyhedron].count; j++) {
            Polygon* poly = &polyhedra[current_polyhedron].faces[j];

            if (poly->count < 3) {
                continue;
            }

            Polygon* new_poly = push_polygon(&polyhedra[1 - current_polyhedron]);
            new_poly->count = 0;
            new_poly->normal = poly->normal;
            new_poly->tex_info = poly->tex_info;

            for (size_t k = 0; k < poly->count; k++) {
                glm::vec3 current_point = poly->vertices[k];
                glm::vec3 prev_point = poly->vertices[(k - 1 + poly->count) % poly->count];

                bool is_current_inside = is_inside_half_plane(plane, current_point);
                bool is_prev_inside = is_inside_half_plane(plane, prev_point);

                if (is_current_inside) {
                    if (!is_prev_inside) {
                        glm::vec3 intersection = edge_plane_intersection(plane, prev_point, current_point);

                        bool is_duplicate = false;
                        for (size_t l = 0; l < clipped_poly.count; l++) {
                            if (glm::all(glm::epsilonEqual(intersection, clipped_poly.vertices[l], DISTEPSILON))) {
                                is_duplicate = true;
                                break;
                            }
                        }
                        if (!is_duplicate) {
                            clipped_poly.vertices[clipped_poly.count++] = intersection;
                        }

                        new_poly->vertices[new_poly->count++] = intersection;
                    }

                    new_poly->vertices[new_poly->count++] = current_point;
                } else if (is_prev_inside) {
                    glm::vec3 intersection = edge_plane_intersection(plane, prev_point, current_point);

                    bool is_duplicate = false;
                    for (size_t l = 0; l < clipped_poly.count; l++) {
                        if (glm::all(glm::epsilonEqual(intersection, clipped_poly.vertices[l], DISTEPSILON))) {
                            is_duplicate = true;
                            break;
                        }
                    }
                    if (!is_duplicate) {
                        clipped_poly.vertices[clipped_poly.count++] = intersection;
                    }

                    new_poly->vertices[new_poly->count++] = intersection;
                }
            }
        }

        if (clipped_poly.count >= 32) {
            fprintf(stderr, "ERROR: clipped_poly overflow\n");
            assert(false);
        }

        if (clipped_poly.count >= 3) {
            convex_polygon_compare_user_data.normal = plane.normal;
            convex_polygon_compare_user_data.center = glm::vec3(0.0f);

            for (size_t j = 0; j < clipped_poly.count; j++) {
                convex_polygon_compare_user_data.center += clipped_poly.vertices[j];
            }
            convex_polygon_compare_user_data.center /= (float)clipped_poly.count;

            qsort(clipped_poly.vertices, clipped_poly.count, sizeof(glm::vec3), convex_polygon_compare);

            Polygon* new_poly = push_polygon(&polyhedra[1 - current_polyhedron]);
            new_poly->count = clipped_poly.count;

            for (size_t j = 0; j < clipped_poly.count; j++) {
                new_poly->vertices[j] = clipped_poly.vertices[j];
            }

            new_poly->normal = plane.normal;
            new_poly->tex_info.offset = plane.offset;
            new_poly->tex_info.scale = plane.scale;
            new_poly->tex_info.rotation = plane.rotation;
        }

        current_polyhedron = (current_polyhedron + 1) % 2;
    }

    Polyhedron* output_polyhedron = &polyhedra[current_polyhedron];

    int faces_upper_bound = 500;
    int vertices_upper_bound = 500;

    // for (size_t i = 0; i < output_polyhedron->count; i++) {
    //     faces_upper_bound += (int)output_polyhedron->faces[i].count - 2;
    //     vertices_upper_bound += (int)output_polyhedron->faces[i].count;
    // }

    MeshData mesh = {};

    mesh.indices = arena.PushArray<uint16_t>(faces_upper_bound * 3);
    mesh.vertices = arena.PushArray<Vertex>(vertices_upper_bound);

    for (size_t i = 0; i < output_polyhedron->count; i++) {
        Polygon* poly = &output_polyhedron->faces[i];

        if ((int)mesh.indices_count >= faces_upper_bound) {
            printf("ERROR: faces_count (%zu) >= faces_upper_bound (%d)\n",
                mesh.indices_count, faces_upper_bound);
            printf("Polygon %zu has %zu vertices\n", i, poly->count);
            assert(false);
        }

        if (poly->count < 3)
            continue;

        int first_vertex_index = find_or_insert_vertex(&mesh, poly->vertices[0], poly->tex_info, poly->normal);

        for (size_t j = 1; j < poly->count - 1; j++) {
            int second_vertex_index = find_or_insert_vertex(&mesh, poly->vertices[j], poly->tex_info, poly->normal);
            int third_vertex_index = find_or_insert_vertex(&mesh, poly->vertices[j + 1], poly->tex_info, poly->normal);

            mesh.indices[mesh.indices_count++] = first_vertex_index;
            mesh.indices[mesh.indices_count++] = second_vertex_index;
            mesh.indices[mesh.indices_count++] = third_vertex_index;
        }
    }

    return mesh;
}