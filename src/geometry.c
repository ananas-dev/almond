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

Polygon* push_polygon(Polyhedron* polyhedron)
{
    return &polyhedron->faces[polyhedron->count++];
}

// O(n)
static int find_or_insert_vertex(Mesh* mesh, vec3 vertex)
{
    for (size_t i = 0; i < mesh->vertices_count; i++) {
        if (glm_vec3_eqv(mesh->vertices[i], vertex)) {
            return (int)i;
        }
    }

    int index = (int)mesh->vertices_count;

    glm_vec3_copy(vertex, mesh->vertices[index]);
    mesh->vertices_count++;

    return index;
}

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
    return glm_vec3_dot(x_minus_s, plane.normal) >= 0;
}

void edge_plane_intersection(Plane plane, vec3 p, vec3 c, vec3 intersection)
{
    // Calculate intersection directly using plane equation
    vec3 ray_dir;
    glm_vec3_sub(c, p, ray_dir);

    vec3 prev_to_anchor;
    glm_vec3_sub(plane.anchor, p, prev_to_anchor);

    float numerator = glm_vec3_dot(prev_to_anchor, plane.normal);
    float denominator = glm_vec3_dot(ray_dir, plane.normal);

    if (fabsf(denominator) > 1e-6f) {  // Avoid division by near-zero
        float t = numerator / denominator;  // Add this line!
        glm_vec3_copy(p, intersection);
        glm_vec3_muladds(ray_dir, t, intersection);
    } else {
        // Fallback for nearly parallel case
        glm_vec3_lerp(p, c, 0.5f, intersection);
    }
}


typedef struct {
    vec3 center;
    vec3 normal;
} ConvexPolygonCompareUserData;

int SDLCALL convex_polygon_compare(void* userdata, const void* a, const void* b)
{
    ConvexPolygonCompareUserData* data = userdata;
    vec3* A = (vec3*)a;
    vec3* B = (vec3*)b;

    vec3 vec_a, vec_b, cross;
    glm_vec3_sub(*A, data->center, vec_a);
    glm_vec3_sub(*B, data->center, vec_b);
    glm_vec3_cross(vec_a, vec_b, cross);

    float dot = glm_vec3_dot(cross, data->normal);

    if (dot > 0)
        return -1;
    if (dot < 0)
        return 1;

    // Collinear case
    return (glm_vec3_norm2(vec_a) > glm_vec3_norm2(vec_b)) ? 1 : -1;
}

Mesh brush_to_mesh(Brush brush, Arena* arena)
{
    assert(brush.count + 4 <= 16 && "Too many cuts");

    Polyhedron polyhedra[2] = { 0 };

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

        Polygon clipped_poly = {0};

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

        if (clipped_poly.count >= 3) {
            ConvexPolygonCompareUserData qsort_user_data = { 0 };

            glm_vec3_copy(plane.normal, qsort_user_data.normal);

            for (size_t j = 0; j < clipped_poly.count; j++) {
                glm_vec3_add(qsort_user_data.center, clipped_poly.vertices[j], qsort_user_data.center);
            }
            glm_vec3_divs(qsort_user_data.center, (float)clipped_poly.count, qsort_user_data.center);

            SDL_qsort_r(clipped_poly.vertices, clipped_poly.count, sizeof(vec3), convex_polygon_compare, &qsort_user_data);

            Polygon *new_poly = push_polygon(&polyhedra[1 - current_polyhedron]);
            new_poly->count = clipped_poly.count;

            for (size_t j = 0; j < clipped_poly.count; j++) {
                glm_vec3_copy(clipped_poly.vertices[j], new_poly->vertices[j]);
            }
        }

        current_polyhedron = (current_polyhedron + 1) % 2;
    }

    Polyhedron* output_polyhedron = &polyhedra[current_polyhedron];

    int faces_upper_bound = 100;
    int vertices_upper_bound = 100;

    // for (size_t i = 0; i < output_polyhedron->count; i++) {
    //     faces_upper_bound += (int)output_polyhedron->faces[i].count - 2;
    //     vertices_upper_bound += (int)output_polyhedron->faces[i].count;
    // }

    Mesh mesh = { 0 };

    mesh.faces = PushArrayAligned(arena, ivec3, faces_upper_bound);
    mesh.vertices = PushArrayAligned(arena, vec3, vertices_upper_bound);

    for (size_t i = 0; i < output_polyhedron->count; i++) {
        Polygon* poly = &output_polyhedron->faces[i];

        if (mesh.faces_count >= faces_upper_bound) {
            printf("ERROR: faces_count (%zu) >= faces_upper_bound (%d)\n",
                   mesh.faces_count, faces_upper_bound);
            printf("Polygon %zu has %zu vertices\n", i, poly->count);
            assert(false);
        }

        if (poly->count < 3)
            continue;
            // assert(false);

        int first_vertex_index = find_or_insert_vertex(&mesh, poly->vertices[0]);

        for (size_t j = 1; j < poly->count - 1; j++) {
            int second_vertex_index = find_or_insert_vertex(&mesh, poly->vertices[j]);
            int third_vertex_index = find_or_insert_vertex(&mesh, poly->vertices[j + 1]);

            ivec3 face = {
                first_vertex_index,
                second_vertex_index,
                third_vertex_index
            };

            glm_ivec3_copy(face, mesh.faces[mesh.faces_count++]);
        }
    }

    return mesh;
}