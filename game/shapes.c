#include "shapes.h"
#include <math.h>
#include "array.h"

MeshData make_capsule(float radius, float height, int radial_segments, int rings, Arena* arena)
{
    Array(Vector3) vertices;
    Array(uint16_t) indices;

    // Top hemisphere

    ArrayAdd(arena, &vertices, )

    Vector3* top_vertex = &mesh.vertices[mesh.vertices_count++];
    *top_vertex[1] = height * 0.5f + radius;

    for (int r = 0; r < rings + 1; r++) {
        for (int s = 0; s < radial_segments; s++) {
            float y = (float)(rings - r) * (radius / (float)rings) + height / 2.0f;
        }
    }
}
