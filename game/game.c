#include <almond.h>

#include "arena.h"
#include "gltf_loader.h"
#include "map.h"
#include "physics.h"
#include "render_commands.h"

#include <math.h>
#include <stdio.h>

typedef struct {
    Arena transient_arena;
    Arena game_arena;
    Camera camera;
    MeshHandle* meshes;
    PhysicsWorld* physics_world;
    CharacterController* character_controller;
    MeshHandle character_mesh;
    size_t meshes_count;
} GameState;

typedef struct {
    Api* api;
    GameState* game_state;
} MapParsingState;

void load_callback(MapEntity* entity, void* user_data, Arena* temp_arena)
{
    MapParsingState* state = user_data;

    for (size_t i = 0; i < entity->brushes_count; i++) {
        MeshData mesh_data = brush_to_mesh(entity->brushes[i], temp_arena);

        for (int i = 0; i < mesh_data.vertices_count; i++) {
            float temp = mesh_data.vertices[i].y;

            mesh_data.vertices[i].x /= 40.f;
            mesh_data.vertices[i].y = mesh_data.vertices[i].z / 40.f;
            mesh_data.vertices[i].z = -temp / 40.f;
        }

        state->game_state->meshes[state->game_state->meshes_count++] = state->api->create_mesh(&mesh_data);
        BodyID id = create_convex_hull_static_collider(state->game_state->physics_world, &mesh_data, temp_arena);

        printf("%u\n", id);
    }
}

GAME_ITERATE(game_iterate)
{
    GameState* game_state = (GameState*)memory->permanent_storage;

    draw_list->clear_color = (Vector4) {
        0.08f, 0.05f, 0.12f, 1.0f
    };

    if (!memory->is_initialized) {
        game_state->transient_arena = create_arena(memory->transient_storage, memory->transient_storage_size);
        game_state->game_arena = create_arena(memory->permanent_storage + sizeof(GameState), memory->permanent_storage_size - sizeof(GameState));

        game_state->physics_world = create_physics_world(&game_state->game_arena);

        CharacterControllerCreateInfo character_controller_create_info = {
            .mass = 70,
            .max_strength = 100,
        };

        game_state->character_controller = create_character_controller(game_state->physics_world, &character_controller_create_info, &game_state->game_arena);

        character_set_position(game_state->character_controller, (Vector3){0, 100, 0});

        game_state->meshes =  PushArray(&game_state->game_arena, MeshHandle, 500);
        game_state->meshes_count = 0;

        MapParsingState parsing_state = { 0 };
        parsing_state.api = api;
        parsing_state.game_state = game_state;

        const char* map_data = api->load_entire_file("./content/celeste.map", NULL);
        parse_map(map_data, load_callback, &parsing_state, &game_state->transient_arena);

        MeshData character_mesh = load_first_mesh_from_gltf("./content/character.glb", &game_state->transient_arena);
        game_state->character_mesh = api->create_mesh(&character_mesh);

        memory->is_initialized = true;
    }

    Vector2 direction = {};

    if (input->move_up.pressed) {
        direction.y -= 1.0f;
    }
    if (input->move_down.pressed) {
        direction.y += 1.0f;
    }
    if (input->move_left.pressed) {
        direction.x -= 1.0f;
    }
    if (input->move_right.pressed) {
        direction.x += 1.0f;
    }

    direction = vector2_normalize(direction);

    Vector3 velocity = character_get_linear_velocity(game_state->character_controller);

    velocity.x = direction.x * 4.0f;
    velocity.z = direction.y * 4.0f;

    if (!character_is_grounded(game_state->character_controller)) {
        if (velocity.y <= -30.0f) {
            velocity.y = -30.0f;
        } else {
            velocity.y += -9.81f * dt;
        }
    } else {
        velocity.y = 0;
    }

    if (character_is_grounded(game_state->character_controller) && input->move_jump.pressed) {
        velocity.y = 10;
    }

    character_set_linear_velocity(game_state->character_controller, velocity);

    character_update(game_state->physics_world, game_state->character_controller, dt, (Vector3) {0, -0.5f, 0});

    update_physics_world(game_state->physics_world, dt, &game_state->transient_arena);

    Vector3 position = character_get_position(game_state->character_controller);

    // printf("%f %f %f\n", position[0], position[1], position[2]);
    // printf("%f %f %f\n", velocity[0], velocity[1], velocity[2]);

    draw_list->camera.position.x = position.x;
    draw_list->camera.position.y = position.y + 10;
    draw_list->camera.position.z = position.z + 10;

    draw_list->camera.target.x = position.x;
    draw_list->camera.target.y = position.y;
    draw_list->camera.target.z = position.z;

    Transform character_transform = TRANSFORM_IDENTITY_INIT;
    character_transform.position = position;

    push_draw_mesh(draw_list, game_state->character_mesh, character_transform);

    for (size_t i = 0; i < game_state->meshes_count - 1; i++) {
        push_draw_mesh(draw_list, game_state->meshes[i], TRANSFORM_IDENTITY);
    }

    // Reset transient arena
    arena_clear(&game_state->transient_arena);
}
