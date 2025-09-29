#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "almond.h"
#include "arena.h"
#include "geometry.h"

typedef struct PhysicsWorld PhysicsWorld;

typedef struct {
    float mass;
    float max_strength;
    Vector3 shape_offset;
} CharacterControllerCreateInfo;

typedef struct CharacterController CharacterController;

typedef uint32_t BodyID;

typedef struct {
    BodyID body;
} StaticShape;

PhysicsWorld* create_physics_world(Arena* arena);

void update_physics_world(PhysicsWorld* physics_world, float dt, Arena* temp_arena);

BodyID create_convex_hull_static_collider(PhysicsWorld* physics_world, MeshData* mesh, Arena* arena);

CharacterController* create_character_controller(PhysicsWorld* physics_world, CharacterControllerCreateInfo* create_info, Arena* arena);
void character_get_linear_velocity(CharacterController* character, Vector3 velocity);
void character_set_linear_velocity(CharacterController* character, Vector3 velocity);
void character_get_position(CharacterController* character, Vector3 position);
void character_set_position(CharacterController* character, Vector3 position);
bool character_is_grounded(CharacterController* character);
void character_update(PhysicsWorld* physics_world, CharacterController* character, float dt, Vector3 gravity);

void physics_step(PhysicsWorld* world);

#ifdef __cplusplus
}
#endif
