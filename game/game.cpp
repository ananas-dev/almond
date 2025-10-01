#include <almond.h>

#include "arena.h"
#include "gltf_loader.h"
#include "map.h"
#include "physics.h"
#include "render_commands.h"
#include "shapes.h"
#include "texture.h"

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
    MeshHandle character_capsule_mesh;
    size_t meshes_count;

    TextureHandle test_texture;

    float camera_yaw;
    float camera_pitch;
    float camera_distance;
} GameState;

typedef struct {
    Api* api;
    GameState* game_state;
} MapParsingState;

void load_callback(MapEntity* entity, void* user_data, Arena& temp_arena)
{
    MapParsingState* state = (MapParsingState*)user_data;

    for (size_t i = 0; i < entity->brushes_count; i++) {
        MeshData mesh_data = brush_to_mesh(entity->brushes[i], temp_arena);

        for (size_t i = 0; i < mesh_data.vertices_count; i++) {
            float temp = mesh_data.vertices[i].position.y;

            mesh_data.vertices[i].position.x /= 40.f;
            mesh_data.vertices[i].position.y = mesh_data.vertices[i].position.z / 40.f;
            mesh_data.vertices[i].position.z = -temp / 40.f;
        }

        state->game_state->meshes[state->game_state->meshes_count++] = state->api->create_mesh(&mesh_data);
        BodyID id = create_convex_hull_static_collider(state->game_state->physics_world, &mesh_data, temp_arena);

        printf("%u\n", id);
    }
}

extern "C" GAME_ITERATE(game_iterate)
{
    auto* game_state = static_cast<GameState*>(memory->permanent_storage);

    draw_list->clear_color = glm::vec4(0.08f, 0.05f, 0.12f, 1.0f);

    if (!memory->is_initialized) {
        game_state->transient_arena = Arena(memory->transient_storage, memory->transient_storage_size);

        // Get pointer after GameState for additional data
        void* after_game_state = static_cast<uint8_t*>(memory->permanent_storage) + sizeof(GameState);
        size_t remaining = memory->permanent_storage_size - sizeof(GameState);

        game_state->game_arena = Arena(after_game_state, remaining);

        game_state->physics_world = create_physics_world(game_state->game_arena);

        CharacterControllerCreateInfo character_controller_create_info = {
            .mass = 70,
            .max_strength = 100,
        };

        game_state->character_controller = create_character_controller(game_state->physics_world, &character_controller_create_info, game_state->game_arena);

        character_set_position(game_state->character_controller, glm::vec3(0, 100, 0));

        game_state->meshes = game_state->game_arena.PushArray<MeshHandle>(500);
        game_state->meshes_count = 0;

        MapParsingState parsing_state = {};
        parsing_state.api = api;
        parsing_state.game_state = game_state;

        const char* map_data = (const char*)api->load_entire_file("./content/celeste.map", NULL);
        parse_map(map_data, load_callback, &parsing_state, game_state->transient_arena);

        // MeshData character_mesh = load_first_mesh_from_gltf("./content/character.glb", &game_state->transient_arena);
        // game_state->character_mesh = api->create_mesh(&character_mesh);

        MeshData capsule_mesh = make_capsule(0.3f, 0.4f, 12, 6, &game_state->transient_arena);
        game_state->character_capsule_mesh = api->create_mesh(&capsule_mesh);

        // Initialize camera state
        game_state->camera_yaw = 0.0f;
        game_state->camera_pitch = -30.0f;
        game_state->camera_distance = 10.0f;

        game_state->test_texture = load_texture("wall.png", api);

        memory->is_initialized = true;
    }

    const float mouse_sensitivity = 0.15f;
    game_state->camera_yaw -= input->mouse_movement.x * mouse_sensitivity;
    game_state->camera_pitch -= input->mouse_movement.y * mouse_sensitivity;

    // Clamp pitch to prevent camera flipping
    const float pitch_min = -89.0f;
    const float pitch_max = 89.0f;
    if (game_state->camera_pitch < pitch_min) {
        game_state->camera_pitch = pitch_min;
    }
    if (game_state->camera_pitch > pitch_max) {
        game_state->camera_pitch = pitch_max;
    }

    // Calculate camera forward direction from angles
    float yaw_rad = game_state->camera_yaw * ((float)M_PI / 180.0f);
    float pitch_rad = game_state->camera_pitch * ((float)M_PI / 180.0f);

    glm::vec3 camera_forward = glm::vec3(
        cosf(pitch_rad) * sinf(yaw_rad),
        sinf(pitch_rad),
        cosf(pitch_rad) * cosf(yaw_rad));

    glm::vec2 direction = glm::vec2(0.0f);

    if (input->move_up.pressed) {
        direction.y += 1.0f;
    }
    if (input->move_down.pressed) {
        direction.y -= 1.0f;
    }
    if (input->move_left.pressed) {
        direction.x += 1.0f;
    }
    if (input->move_right.pressed) {
        direction.x -= 1.0f;
    }

    if (glm::length(direction) > 1e-6)
        direction = glm::normalize(direction);

    // Transform direction based on camera yaw
    float cos_yaw = cosf(yaw_rad);
    float sin_yaw = sinf(yaw_rad);

    // Rotate the direction vector by the camera's yaw
    glm::vec3 world_direction = glm::vec3(
        direction.x * cos_yaw + direction.y * sin_yaw,
        0.0f,
        -direction.x * sin_yaw + direction.y * cos_yaw);

    glm::vec3 velocity = character_get_linear_velocity(game_state->character_controller);

    float speed = 12.0f;

    velocity.x = world_direction.x * speed;
    velocity.z = world_direction.z * speed;

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

    character_update(game_state->physics_world, game_state->character_controller, dt, glm::vec3(0, -0.5f, 0));
    update_physics_world(game_state->physics_world, dt, game_state->transient_arena);

    glm::vec3 position = character_get_position(game_state->character_controller);

    // Position camera behind and above the character
    glm::vec3 camera_offset = {
        -camera_forward.x * game_state->camera_distance,
        -camera_forward.y * game_state->camera_distance,
        -camera_forward.z * game_state->camera_distance
    };

    draw_list->camera.position = position + camera_offset;
    draw_list->camera.target = position;

    Transform character_transform;
    character_transform.position = position;

    Transform world_transform;

    for (size_t i = 0; i < game_state->meshes_count - 1; i++) {
        push_draw_mesh(draw_list, game_state->meshes[i], game_state->test_texture, world_transform);
    }

    push_draw_mesh(draw_list, game_state->character_capsule_mesh, game_state->test_texture, character_transform);

    // Reset transient arena
    game_state->transient_arena.clear();
}
