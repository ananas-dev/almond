#include "shapes.h"
#include "list.h"

MeshData make_capsule(float radius, float cylinder_half_height, int radial_segments, int rings, Arena* arena)
{
    List<glm::vec3, Arena> vertices(arena);
    List<uint16_t, Arena> indices(arena);

    // Top hemisphere
   vertices.push(glm::vec3(0, cylinder_half_height + radius, 0));

    for (int r = 1; r < rings; r++) {
        for (int s = 0; s < radial_segments; s++) {
            float theta = (float)M_PI_2 * (float)r / (float)rings;
            float phi = 2.0f * (float)M_PI * (float)s / (float)radial_segments;

            float sin_theta = sinf(theta);
            vertices.push(glm::vec3(radius * sin_theta * cosf(phi), radius * cosf(theta) + cylinder_half_height, radius * sin_theta * sinf(phi)));
        }
    }

    // Bottom hemisphere
    for (int r = rings - 1; r > 0; r--) {
        for (int s = 0; s < radial_segments; s++) {
            float theta = (float)M_PI_2 * (float)r / (float)rings;
            float phi = 2.0f * (float)M_PI * (float)s / (float)radial_segments;

            float sin_theta = sinf(theta);
            vertices.push(glm::vec3(radius * sin_theta * cosf(phi), -(radius * cosf(theta) + cylinder_half_height), radius * sin_theta * sinf(phi)));
        }
    }

    vertices.push(glm::vec3(0, -(cylinder_half_height + radius), 0));

    // Top hemisphere

    for (int s = 0; s < radial_segments; s++) {
        indices.push(0);
        indices.push(s + 1);
        indices.push(((s + 1) % radial_segments) + 1);
    }

    for (int r = 0; r < 2 * rings - 3; r++) {
        for (int s = 0; s < radial_segments; s++) {
            int current = r * radial_segments + s + 1;
            int next = r * radial_segments + ((s + 1) % radial_segments) + 1;
            int current_below = (r + 1) * radial_segments + s + 1;
            int next_below = (r + 1) * radial_segments + ((s + 1) % radial_segments) + 1;

            indices.push(current);
            indices.push(current_below);
            indices.push(next_below);

            indices.push(next_below);
            indices.push(next);
            indices.push(current);
        }
    }

    for (int s = 0; s < radial_segments; s++) {
        indices.push(vertices.count - 1);
        indices.push(vertices.count - 1 - radial_segments + ((s + 1) % radial_segments));
        indices.push(vertices.count - 1 - radial_segments + s);
    }

    MeshData mesh = {};
    mesh.indices = indices.items;
    mesh.indices_count = indices.count;

    mesh.vertices = arena->PushArray<Vertex>(vertices.count);
    mesh.vertices_count = vertices.count;

    for (size_t i = 0; i < vertices.count; i++) {
        mesh.vertices[i].position = vertices[i];
        mesh.vertices->texcoords = glm::vec3(0.0f);
    }

    return mesh;
}
