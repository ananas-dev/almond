#include "gltf_loader.h"
#include "arena.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <stdio.h>
#include <assert.h>

#include <cglm/cglm.h>

// typedef struct {
//     vec3 position;
//     vec3 normal;
//     vec2 uv_coord;
// } Vertex;

MeshData load_first_mesh_from_gltf(const char* path, Arena* arena) {
    MeshData output_mesh = {0};

    cgltf_options options = {0};
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, path, &data);
    cgltf_load_buffers(&options, data, path);

    assert(result == cgltf_result_success);
    assert(data->scene != NULL);

    assert(data->meshes_count >= 1);

    // Find first node with a mesh
    cgltf_mesh mesh = data->meshes[0];

    assert(mesh.primitives_count == 1);

    cgltf_primitive primitive = mesh.primitives[0];

    // assert(primitive.attributes_count == 3);

    cgltf_attribute* positions = NULL;
    cgltf_attribute* normals = NULL;
    cgltf_attribute* uv_coords = NULL;

    for (cgltf_size i = 0; i < 3; i++) {
        switch (primitive.attributes[i].type) {
            case cgltf_attribute_type_position:
                positions = &primitive.attributes[i];
                break;
            case cgltf_attribute_type_normal:
                normals = &primitive.attributes[i];
                break;
            case cgltf_attribute_type_texcoord:
                uv_coords = &primitive.attributes[i];
                break;
            default:
                break; // TODO
                // assert(false);
        }
    }

    // assert(positions != NULL && normals != NULL && uv_coords != NULL);
    assert(positions != NULL);

    output_mesh.vertices_count = positions->data->count;
    output_mesh.vertices = PushArray(arena, Vertex, output_mesh.vertices_count);

    for (int i = 0; i < output_mesh.vertices_count; i++) {
        Vertex* vertex = &output_mesh.vertices[i];

        cgltf_accessor_read_float(positions->data, i, vertex->position.items, 3);
        memset(vertex->texcoords.items, 0, 2 * sizeof(float));
        // cgltf_accessor_read_float(normals->data, i, vertex->normal, 3);
        // cgltf_accessor_read_float(uv_coords->data, i, vertex->texcoords.items, 2);
    }

    output_mesh.indices_count = primitive.indices->count;
    output_mesh.indices = PushArray(arena, uint16_t, output_mesh.indices_count);

    for (int i = 0; i < output_mesh.indices_count; i++) {
        output_mesh.indices[i] = cgltf_accessor_read_index(primitive.indices, i);
    }

    cgltf_free(data);

    return output_mesh;
}
