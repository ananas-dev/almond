#include "geometry.h"

#include <cglm/cglm.h>
#include <stdlib.h>

#define DISTEPSILON 1e-6

typedef struct {
    vec3 vertices[32]; // Edges upper-bound
    size_t count;
} Polygon;

typedef struct {
    Polygon* faces;
    size_t count;
} Polyhedron;

static vec3 unit_cube_vertices[8] = {
    { -0.5f, -0.5f, 0.5f }, // 0
    { 0.5f, -0.5f, 0.5f }, // 1
    { -0.5f, 0.5f, 0.5f }, // 2
    { 0.5f, 0.5f, 0.5f }, // 3
    { -0.5f, -0.5f, -0.5f }, // 4
    { 0.5f, -0.5f, -0.5f }, // 5
    { -0.5f, 0.5f, -0.5f }, // 6
    { 0.5f, 0.5f, -0.5f } // 7
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

Polygon* push_polygon(Polyhedron* polyhedron)
{
    return &polyhedron->faces[polyhedron->count++];
}

// O(n)
static int find_or_insert_vertex(MeshData* mesh, vec3 vertex)
{
    // Quake coordinates -> Y-up coordinates
    vec3 yup_vertex;

    yup_vertex[0] = vertex[0];
    yup_vertex[1] = vertex[2];
    yup_vertex[2] = -vertex[1];

    for (size_t i = 0; i < mesh->vertices_count; i++) {
        if (glm_vec3_eqv(mesh->vertices[i], yup_vertex)) {
            return (int)i;
        }
    }

    int index = (int)mesh->vertices_count;

    glm_vec3_copy(yup_vertex, mesh->vertices[index]);
    mesh->vertices_count++;

    return index;
}

Plane plane_from_points(Vector3 a, Vector3 b, Vector3 c)
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
    return glm_vec3_dot(x_minus_s, plane.normal) >= 0;
}

void edge_plane_intersection(Plane plane, vec3 p, vec3 c, vec3 intersection)
{
    vec3 ray_dir;
    glm_vec3_sub(c, p, ray_dir);

    vec3 prev_to_anchor;
    glm_vec3_sub(plane.anchor, p, prev_to_anchor);

    float numerator = glm_vec3_dot(prev_to_anchor, plane.normal);
    float denominator = glm_vec3_dot(ray_dir, plane.normal);

    if (fabsf(denominator) > DISTEPSILON) {
        float t = numerator / denominator;
        glm_vec3_copy(p, intersection);
        glm_vec3_muladds(ray_dir, t, intersection);
    } else {
        // Fallback
        glm_vec3_lerp(p, c, 0.5f, intersection);
    }
}

typedef struct {
    vec3 center;
    vec3 normal;
} ConvexPolygonCompareUserData;

static ConvexPolygonCompareUserData convex_polygon_compare_user_data;

int convex_polygon_compare(const void* a, const void* b)
{
    ConvexPolygonCompareUserData* data = &convex_polygon_compare_user_data;
    vec3* A = (vec3*)a;
    vec3* B = (vec3*)b;

    vec3 vec_a, vec_b;
    glm_vec3_sub(*A, data->center, vec_a);
    glm_vec3_sub(*B, data->center, vec_b);

    // Project vectors onto the plane
    float dot_a = glm_vec3_dot(vec_a, data->normal);
    float dot_b = glm_vec3_dot(vec_b, data->normal);

    vec3 proj_a, proj_b;
    glm_vec3_copy(vec_a, proj_a);
    glm_vec3_muladds(data->normal, -dot_a, proj_a);

    glm_vec3_copy(vec_b, proj_b);
    glm_vec3_muladds(data->normal, -dot_b, proj_b);

    // Normalize projected vectors
    glm_vec3_normalize(proj_a);
    glm_vec3_normalize(proj_b);

    // Calculate angles using atan2
    // We need a consistent reference frame on the plane
    vec3 reference, tangent;

    // Create reference vector perpendicular to normal
    if (fabsf(data->normal[0]) < 0.9f) {
        glm_vec3_cross(data->normal, (vec3){1,0,0}, reference);
    } else {
        glm_vec3_cross(data->normal, (vec3){0,1,0}, reference);
    }
    glm_vec3_normalize(reference);

    // Create tangent vector (completes the 2D coordinate system)
    glm_vec3_cross(data->normal, reference, tangent);

    // Get 2D coordinates on the plane
    float angle_a = atan2f(glm_vec3_dot(proj_a, tangent), glm_vec3_dot(proj_a, reference));
    float angle_b = atan2f(glm_vec3_dot(proj_b, tangent), glm_vec3_dot(proj_b, reference));

    if (angle_a < angle_b)
        return -1;
    if (angle_a > angle_b)
        return 1;

    return 0;
}

MeshData brush_to_mesh(Brush brush, Arena* arena)
{
    Polyhedron polyhedra[2] = { 0 };

    polyhedra[0].faces = PushArray(arena, Polygon, 6 + brush.count);
    polyhedra[1].faces = PushArray(arena, Polygon, 6 + brush.count);

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

        Polygon clipped_poly = {};

        for (size_t j = 0; j < polyhedra[current_polyhedron].count; j++) {
            Polygon* poly = &polyhedra[current_polyhedron].faces[j];

            if (poly->count < 3) {
                continue;
            }

            Polygon* new_poly = push_polygon(&polyhedra[1 - current_polyhedron]);
            new_poly->count = 0;

            for (size_t k = 0; k < poly->count; k++) {
                vec3 current_point;
                glm_vec3_copy(poly->vertices[k], current_point);

                vec3 prev_point;
                glm_vec3_copy(poly->vertices[(k - 1 + poly->count) % poly->count], prev_point);

                bool is_current_inside = is_inside_half_plane(plane, current_point);
                bool is_prev_inside = is_inside_half_plane(plane, prev_point);

                if (is_current_inside) {
                    if (!is_prev_inside) {
                        vec3 intersection;
                        edge_plane_intersection(plane, prev_point, current_point, intersection);

                        bool is_duplicate = false;
                        for (size_t l = 0; l < clipped_poly.count; l++) {
                            if (glm_vec3_eqv(intersection, clipped_poly.vertices[l])) {
                                is_duplicate = true;
                                break;
                            }
                        }
                        if (!is_duplicate) {
                            glm_vec3_copy(intersection, clipped_poly.vertices[clipped_poly.count++]);
                        }

                        glm_vec3_copy(intersection, new_poly->vertices[new_poly->count++]);
                    }

                    glm_vec3_copy(current_point, new_poly->vertices[new_poly->count++]);
                } else if (is_prev_inside) {
                    vec3 intersection;
                    edge_plane_intersection(plane, prev_point, current_point, intersection);

                    bool is_duplicate = false;
                    for (size_t l = 0; l < clipped_poly.count; l++) {
                        if (glm_vec3_eqv(intersection, clipped_poly.vertices[l])) {
                            is_duplicate = true;
                            break;
                        }
                    }
                    if (!is_duplicate) {
                        glm_vec3_copy(intersection, clipped_poly.vertices[clipped_poly.count++]);
                    }

                    glm_vec3_copy(intersection, new_poly->vertices[new_poly->count++]);
                }
            }
        }

        if (clipped_poly.count >= 32) {
            fprintf(stderr, "ERROR: clipped_poly overflow\n");
            assert(false);
        }

        if (clipped_poly.count >= 3) {
            glm_vec3_copy(plane.normal, convex_polygon_compare_user_data.normal);

            for (size_t j = 0; j < clipped_poly.count; j++) {
                glm_vec3_add(convex_polygon_compare_user_data.center, clipped_poly.vertices[j], convex_polygon_compare_user_data.center);
            }
            glm_vec3_divs(convex_polygon_compare_user_data.center, (float)clipped_poly.count, convex_polygon_compare_user_data.center);

            qsort(clipped_poly.vertices, clipped_poly.count, sizeof(vec3), convex_polygon_compare);

            Polygon* new_poly = push_polygon(&polyhedra[1 - current_polyhedron]);
            new_poly->count = clipped_poly.count;

            for (size_t j = 0; j < clipped_poly.count; j++) {
                glm_vec3_copy(clipped_poly.vertices[j], new_poly->vertices[j]);
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

    MeshData mesh = { 0 };

    mesh.indices = PushArray(arena, uint16_t, faces_upper_bound * 3);
    mesh.vertices = PushArray(arena, vec3, vertices_upper_bound);

    for (size_t i = 0; i < output_polyhedron->count; i++) {
        Polygon* poly = &output_polyhedron->faces[i];

        if (mesh.indices_count >= faces_upper_bound) {
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