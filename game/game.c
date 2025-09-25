#include <almond.h>

#include "arena.h"
#include "map.h"
#include "render_commands.h"

#include <math.h>
#include <stdio.h>

typedef struct {
    Arena transient_arena;
    Camera camera;
    MeshHandle* meshes;
    size_t meshes_count;
} GameState;

typedef struct {
    Api* api;
    GameState* game_state;
} MapParsingState;

void load_callback(MapEntity* entity, void* user_data, Arena* temp_arena)
{
    MapParsingState* state = user_data;

    printf("%.*s\n", (int)entity->classname_size, entity->classname);

    for (size_t i = 0; i < entity->brushes_count; i++) {
        MeshData mesh_data = brush_to_mesh(entity->brushes[i], temp_arena);
        printf("vertices: %lu, indices: %lu\n", mesh_data.indices_count, mesh_data.vertices_count);
        state->game_state->meshes[state->game_state->meshes_count++] = state->api->create_mesh(&mesh_data);
    }
}

float clamp(float value, float min, float max)
{
    if (value < min)
        return min;
    if (value > max)
        return max;
    return value;
}

float to_radians(float degrees)
{
    return degrees * (M_PI / 180.0f);
}

void update_camera_from_mouse(Camera* camera, float xoff, float yoff, float sensitivity)
{
    // Update angles
    camera->yaw += xoff * sensitivity;
    camera->pitch -= yoff * sensitivity; // Invert Y for natural feel

    // Clamp pitch to prevent gimbal lock
    camera->pitch = clamp(camera->pitch, -89.0f, 89.0f);

    // Convert angles to forward vector
    float yaw_rad = to_radians(camera->yaw);
    float pitch_rad = to_radians(camera->pitch);

    float forward[3];
    forward[0] = cosf(yaw_rad) * cosf(pitch_rad);
    forward[1] = sinf(pitch_rad);
    forward[2] = sinf(yaw_rad) * cosf(pitch_rad);

    // Set target = position + forward
    camera->target[0] = camera->position[0] + forward[0];
    camera->target[1] = camera->position[1] + forward[1];
    camera->target[2] = camera->position[2] + forward[2];
}

GAME_ITERATE(game_iterate)
{
    GameState* game_state = (GameState*)memory->permanent_storage;

    draw_list->clear_color = (Color) {
        0.08f, 0.05f, 0.12f, 1.0f
    };

    update_camera_from_mouse(&game_state->camera, input->mouse_movement[0], input->mouse_movement[1], 1.0f);

    draw_list->camera = game_state->camera;

    if (!memory->is_initialized) {
        game_state->transient_arena = (Arena) {
            .base = memory->transient_storage,
            .size = memory->permanent_storage_size,
        };

        game_state->meshes = memory->permanent_storage + sizeof(GameState);
        game_state->meshes_count = 0;

        MapParsingState parsing_state = { 0 };
        parsing_state.api = api;
        parsing_state.game_state = game_state;

        const char* map_data = api->load_entire_file("./content/celeste.map", NULL);
        parse_map(map_data, load_callback, &parsing_state, &game_state->transient_arena);

        game_state->camera = (Camera) {
            .position = { -200, 700, 0 },
            .target = { 1, 0, 0 },
        };

        memory->is_initialized = true;
    }

    for (size_t i = 0; i < game_state->meshes_count - 1; i++) {
        push_draw_mesh(draw_list, game_state->meshes[i], TRANSFORM_IDENTITY);
    }

    // Reset transient arena
    game_state->transient_arena.current = 0;
}
