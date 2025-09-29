#include "geometry.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define DISTEPSILON 1e-6
#define GRID_SIZE 1e-2

typedef struct {
    Vector3 vertices[32]; // Edges upper-bound
    size_t count;
} Polygon;

typedef struct {
    Polygon* faces;
    size_t count;
} Polyhedron;

static Vector3 unit_cube_vertices[8] = {
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

Polygon* push_polygon(Polyhedron* polyhedron)
{
    return &polyhedron->faces[polyhedron->count++];
}

static Vector3 snap_to_grid(Vector3 vertex, float grid_size)
{
    return (Vector3)
    {
        roundf(vertex.x / grid_size) * grid_size,
            roundf(vertex.y / grid_size) * grid_size,
            roundf(vertex.z / grid_size) * grid_size,
    };
}

// O(n)
static int find_or_insert_vertex(MeshData* mesh, Vector3 vertex)
{
    Vector3 snapped_vertex = snap_to_grid(vertex, GRID_SIZE);

    for (size_t i = 0; i < mesh->vertices_count; i++) {
        if (vector3_eq(mesh->vertices[i], snapped_vertex, DISTEPSILON)) {
            return (int)i;
        }
    }

    int index = (int)mesh->vertices_count;

    mesh->vertices[index] = snapped_vertex;
    mesh->vertices_count++;

    return index;
}

Plane plane_from_points(Vector3 a, Vector3 b, Vector3 c)
{
    Plane plane = { 0 };

    plane.normal = vector3_normalize(vector3_cross(vector3_sub(b, a), vector3_sub(c, a)));
    plane.anchor = a;

    return plane;
}

static bool is_inside_half_plane(Plane plane, Vector3 x)
{
    return vector3_dot(vector3_sub(x, plane.anchor), plane.normal) >= 0;
}

Vector3 edge_plane_intersection(Plane plane, Vector3 p, Vector3 c)
{
    Vector3 ray_dir = vector3_sub(c, p);
    Vector3 prev_to_anchor = vector3_sub(plane.anchor, p);

    float numerator = vector3_dot(prev_to_anchor, plane.normal);
    float denominator = vector3_dot(ray_dir, plane.normal);

    Vector3 intersection;
    if (fabsf(denominator) > DISTEPSILON) {
        float t = numerator / denominator;
        intersection = vector3_add(p, vector3_scale(ray_dir, t));
    } else {
        // Fallback
        intersection = vector3_lerp(p, c, 0.5f);
    }

    return snap_to_grid(intersection, GRID_SIZE);
}

typedef struct {
    Vector3 center;
    Vector3 normal;
} ConvexPolygonCompareUserData;

static ConvexPolygonCompareUserData convex_polygon_compare_user_data;

int convex_polygon_compare(const void* a, const void* b)
{
    ConvexPolygonCompareUserData* data = &convex_polygon_compare_user_data;
    Vector3* A = (Vector3*)a;
    Vector3* B = (Vector3*)b;

    Vector3 vec_a = vector3_sub(*A, data->center);
    Vector3 vec_b = vector3_sub(*B, data->center);

    // Project vectors onto the plane
    float dot_a = vector3_dot(vec_a, data->normal);
    float dot_b = vector3_dot(vec_b, data->normal);

    Vector3 proj_a = vector3_sub(vec_a, vector3_scale(data->normal, dot_a));
    Vector3 proj_b = vector3_sub(vec_b, vector3_scale(data->normal, dot_b));

    // Normalize projected vectors
    proj_a = vector3_normalize(proj_a);
    proj_b = vector3_normalize(proj_b);

    // Calculate angles using atan2
    // We need a consistent reference frame on the plane
    Vector3 reference;

    // Create reference vector perpendicular to normal
    if (fabsf(data->normal.x) < 0.9f) {
        reference = vector3_cross(data->normal, VEC3(1, 0, 0));
    } else {
        reference = vector3_cross(data->normal, VEC3(0, 1, 0));
    }
    reference = vector3_normalize(reference);

    // Create tangent vector (completes the 2D coordinate system)
    Vector3 tangent = vector3_cross(data->normal, reference);

    // Get 2D coordinates on the plane
    float angle_a = atan2f(vector3_dot(proj_a, tangent), vector3_dot(proj_a, reference));
    float angle_b = atan2f(vector3_dot(proj_b, tangent), vector3_dot(proj_b, reference));

    if (angle_a < angle_b)
        return -1;
    if (angle_a > angle_b)
        return 1;

    return 0;
}

MeshData brush_to_mesh(Brush brush, Arena* arena)
{
    Polyhedron polyhedra[2] = {};

    polyhedra[0].faces = PushArray(arena, Polygon, 6 + brush.count);
    polyhedra[1].faces = PushArray(arena, Polygon, 6 + brush.count);

    int current_polyhedron = 0;

    // Fill the current polyhedron with a huge cube
    polyhedra[0].count = 6;
    for (int i = 0; i < 6; i++) {
        Polygon* poly = &polyhedra[current_polyhedron].faces[i];

        poly->count = 4;

        for (int j = 0; j < 4; j++) {
            poly->vertices[j] = vector3_scale(unit_cube_vertices[unit_cube_faces[i][j]], 8192);
            poly->vertices[j] = snap_to_grid(poly->vertices[j], GRID_SIZE);
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

            for (size_t k = 0; k < poly->count; k++) {
                Vector3 current_point = poly->vertices[k];
                Vector3 prev_point = poly->vertices[(k - 1 + poly->count) % poly->count];

                bool is_current_inside = is_inside_half_plane(plane, current_point);
                bool is_prev_inside = is_inside_half_plane(plane, prev_point);

                if (is_current_inside) {
                    if (!is_prev_inside) {
                        Vector3 intersection = edge_plane_intersection(plane, prev_point, current_point);

                        bool is_duplicate = false;
                        for (size_t l = 0; l < clipped_poly.count; l++) {
                            if (vector3_eq(intersection, clipped_poly.vertices[l], DISTEPSILON)) {
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
                    Vector3 intersection = edge_plane_intersection(plane, prev_point, current_point);

                    bool is_duplicate = false;
                    for (size_t l = 0; l < clipped_poly.count; l++) {
                        if (vector3_eq(intersection, clipped_poly.vertices[l], DISTEPSILON)) {
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
            convex_polygon_compare_user_data.center = VEC3_ZERO;

            for (size_t j = 0; j < clipped_poly.count; j++) {
                convex_polygon_compare_user_data.center = vector3_add(convex_polygon_compare_user_data.center, clipped_poly.vertices[j]);
            }
            convex_polygon_compare_user_data.center = vector3_divs(convex_polygon_compare_user_data.center, (float)clipped_poly.count);

            qsort(clipped_poly.vertices, clipped_poly.count, sizeof(Vector3), convex_polygon_compare);

            Polygon* new_poly = push_polygon(&polyhedra[1 - current_polyhedron]);
            new_poly->count = clipped_poly.count;

            for (size_t j = 0; j < clipped_poly.count; j++) {
                new_poly->vertices[j] = clipped_poly.vertices[j];
            }
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

    mesh.indices = PushArray(arena, uint16_t, faces_upper_bound * 3);
    mesh.vertices = PushArray(arena, Vector3, vertices_upper_bound);

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

        int first_vertex_index = find_or_insert_vertex(&mesh, poly->vertices[0]);

        for (size_t j = 1; j < poly->count - 1; j++) {
            int second_vertex_index = find_or_insert_vertex(&mesh, poly->vertices[j]);
            int third_vertex_index = find_or_insert_vertex(&mesh, poly->vertices[j + 1]);

            mesh.indices[mesh.indices_count++] = first_vertex_index;
            mesh.indices[mesh.indices_count++] = second_vertex_index;
            mesh.indices[mesh.indices_count++] = third_vertex_index;
        }
    }

    return mesh;
}