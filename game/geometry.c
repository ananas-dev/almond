#include "geometry.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define DISTEPSILON 1e-6
#define GRID_SIZE 1e-2

typedef struct {
    Vector2 offset;
    Vector2 scale;
    float rotation;
} TexInfo;

typedef struct {
    Vector3 vertices[32]; // Edges upper-bound
    size_t count;
    TexInfo tex_info;
    Vector3 normal;
} Polygon;

typedef struct {
    Polygon* faces;
    size_t count;
} Polyhedron;

static Vector3 unit_cube_vertices[8] = {
    { { -0.5f, -0.5f, 0.5f } }, // 0
    { { 0.5f, -0.5f, 0.5f } }, // 1
    { { -0.5f, 0.5f, 0.5f } }, // 2
    { { 0.5f, 0.5f, 0.5f } }, // 3
    { { -0.5f, -0.5f, -0.5f } }, // 4
    { { 0.5f, -0.5f, -0.5f } }, // 5
    { { -0.5f, 0.5f, -0.5f } }, // 6
    { { 0.5f, 0.5f, -0.5f } } // 7
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

static Vector3 unit_cube_normals[6] = {
    { { 0.0f, 0.0f, 1.0f } }, // Top (z = 0.5)
    { { 0.0f, 0.0f, -1.0f } }, // Bottom (z = -0.5)
    { { -1.0f, 0.0f, 0.0f } }, // Left (x = -0.5)
    { { 1.0f, 0.0f, 0.0f } }, // Right (x = 0.5)
    { { 0.0f, -1.0f, 0.0f } }, // Front (y = -0.5)
    { { 0.0f, 1.0f, 0.0f } } // Back (y = 0.5)
};

Polygon* push_polygon(Polyhedron* polyhedron)
{
    return &polyhedron->faces[polyhedron->count++];
}

static Vector3 snap_to_grid(Vector3 vertex, float grid_size)
{
    return VEC3(
        roundf(vertex.x / grid_size) * grid_size,
        roundf(vertex.y / grid_size) * grid_size,
        roundf(vertex.z / grid_size) * grid_size);
}

// 3x3 Matrix for rotation and transformation
typedef struct {
    float m[3][3];
} Matrix3;

// Create a 3x3 identity matrix
static Matrix3 matrix3_identity()
{
    Matrix3 out = { { { 1, 0, 0 },
        { 0, 1, 0 },
        { 0, 0, 1 } } };
    return out;
}

// Create a 3x3 rotation matrix given an axis and angle (right-handed, angle in radians)
// Rodrigues' rotation formula
static Matrix3 matrix3_axis_angle(Vector3 axis, float angle)
{
    axis = vector3_normalize(axis);
    float x = axis.x, y = axis.y, z = axis.z;
    float c = cosf(angle);
    float s = sinf(angle);
    float t = 1.0f - c;

    Matrix3 out;
    out.m[0][0] = c + x * x * t;
    out.m[0][1] = x * y * t - z * s;
    out.m[0][2] = x * z * t + y * s;

    out.m[1][0] = y * x * t + z * s;
    out.m[1][1] = c + y * y * t;
    out.m[1][2] = y * z * t - x * s;

    out.m[2][0] = z * x * t - y * s;
    out.m[2][1] = z * y * t + x * s;
    out.m[2][2] = c + z * z * t;

    return out;
}

// Transform a vector by a 3x3 matrix
static Vector3 matrix3_transform(Matrix3 m, Vector3 v)
{
    Vector3 out;
    out.x = m.m[0][0] * v.x + m.m[0][1] * v.y + m.m[0][2] * v.z;
    out.y = m.m[1][0] * v.x + m.m[1][1] * v.y + m.m[1][2] * v.z;
    out.z = m.m[2][0] * v.x + m.m[2][1] * v.y + m.m[2][2] * v.z;
    return out;
}

static Vector3 base_axis[6][3] = {
    { { 0, 0, 1 }, { 1, 0, 0 }, { 0, -1, 0 } }, // floor
    { { 0, 0, -1 }, { 1, 0, 0 }, { 0, -1, 0 } }, // ceiling
    { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, -1 } }, // west wall
    { { -1, 0, 0 }, { 0, 1, 0 }, { 0, 0, -1 } }, // east wall
    { { 0, 1, 0 }, { 1, 0, 0 }, { 0, 0, -1 } }, // south wall
    { { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, -1 } } // north wall
};

static void calculate_rotated_uv(
    Vector3 normal,
    Vector3 u_axis_in,
    Vector3 v_axis_in,
    float xscale,
    float yscale,
    float rotation_deg,
    Vector3* u_axis_out,
    Vector3* v_axis_out)
{
    // 1. Scale - preserve the sign for flipping
    Vector3 scaled_u = vector3_scale(u_axis_in, 1.0f / xscale);
    Vector3 scaled_v = vector3_scale(v_axis_in, 1.0f / yscale);

    // 2. Rotation axis is the dominant axis of the normal
    Vector3 rotation_axis;
    Vector3 abs_n = vector3_abs(normal);
    if (abs_n.x > abs_n.y && abs_n.x > abs_n.z)
        rotation_axis = VEC3(1, 0, 0);
    else if (abs_n.y > abs_n.z)
        rotation_axis = VEC3(0, 1, 0);
    else
        rotation_axis = VEC3(0, 0, 1);

    // 3. Apply rotation (if any)
    if (fabsf(rotation_deg) > 0.001f) {
        float rotation_rad = rotation_deg * (float)M_PI / 180.0f;
        Matrix3 rotmat = matrix3_axis_angle(rotation_axis, rotation_rad);
        *u_axis_out = matrix3_transform(rotmat, scaled_u);
        *v_axis_out = matrix3_transform(rotmat, scaled_v);
    } else {
        *u_axis_out = scaled_u;
        *v_axis_out = scaled_v;
    }
}

static Vector2 calculate_uv(
    Vector3 vertex,
    float xshift, float yshift,
    float tex_width, float tex_height,
    Vector3 u_axis, Vector3 v_axis)
{
    float u = vector3_dot(vertex, u_axis) + xshift;
    float v = vector3_dot(vertex, v_axis) + yshift;
    u /= tex_width;
    v /= tex_height;
    return VEC2(u, v);
}

// O(n)
static int find_or_insert_vertex(MeshData* mesh, Vector3 position, TexInfo tex_info, Vector3 normal)
{
    Vector3 snapped_position = snap_to_grid(position, GRID_SIZE);

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
        float dot = vector3_dot(normal, base_axis[i][0]);
        if (dot > best_dot) {
            best_dot = dot;
            bestaxis = i;
        }
    }

    Vector3 u_axis = base_axis[bestaxis][1];
    Vector3 v_axis = base_axis[bestaxis][2];

    Vector3 rotated_u, rotated_v;
    calculate_rotated_uv(
        normal,
        u_axis, v_axis,
        tex_info.scale.u, tex_info.scale.v,
        tex_info.rotation,
        &rotated_u, &rotated_v);

    Vector2 uv = calculate_uv(
        snapped_position,
        tex_info.offset.u, tex_info.offset.v,
        32.0f, 32.0f,
        rotated_u, rotated_v);

    mesh->vertices[index].position = snapped_position;
    mesh->vertices[index].texcoords = uv;
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

        // poly->tex_info;
        poly->normal = unit_cube_normals[i];

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
            new_poly->normal = poly->normal;
            new_poly->tex_info = poly->tex_info;

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

    mesh.indices = PushArray(arena, uint16_t, faces_upper_bound * 3);
    mesh.vertices = PushArray(arena, Vertex, vertices_upper_bound);

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