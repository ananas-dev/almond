#include "physics.h"

#include <Jolt/Jolt.h>

// Jolt includes
#include "Jolt/Core/JobSystemSingleThreaded.h"
#include "Jolt/Geometry/ConvexHullBuilder.h"
#include "Jolt/Math/Quat.h"
#include "Jolt/Physics/Character/CharacterVirtual.h"
#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"
#include "Jolt/Physics/Collision/Shape/ConvexHullShape.h"

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Math/Real.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhase.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CompoundShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <cassert>

JPH_SUPPRESS_WARNINGS

// Layer that objects can be in, determines which other objects it can collide with
// Typically you at least want to have 1 layer for moving bodies and 1 layer for static bodies, but you can have more
// layers if you want. E.g. you could have a layer for high detail collision (which is not used by the physics simulation
// but only if you do collision testing).
namespace Layers {
static constexpr JPH::ObjectLayer NON_MOVING = 0;
static constexpr JPH::ObjectLayer MOVING = 1;
static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
};

/// Class that determines if two object layers can collide
class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
    {
        switch (inObject1) {
        case Layers::NON_MOVING:
            return inObject2 == Layers::MOVING; // Non moving only collides with moving
        case Layers::MOVING:
            return true; // Moving collides with everything
        default:
            JPH_ASSERT(false);
            return false;
        }
    }
};

// Each broadphase layer results in a separate bounding volume tree in the broad phase. You at least want to have
// a layer for non-moving and moving objects to avoid having to update a tree full of static objects every frame.
// You can have a 1-on-1 mapping between object layers and broadphase layers (like in this case) but if you have
// many object layers you'll be creating many broad phase trees, which is not efficient. If you want to fine tune
// your broadphase layers define JPH_TRACK_BROADPHASE_STATS and look at the stats reported on the TTY.
namespace BroadPhaseLayers {
static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
static constexpr JPH::BroadPhaseLayer MOVING(1);
static constexpr uint NUM_LAYERS(2);
};

// BroadPhaseLayerInterface implementation
// This defines a mapping between object and broadphase layers.
class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    BPLayerInterfaceImpl()
    {
        // Create a mapping table from object to broad phase layer
        mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
    }

    [[nodiscard]] uint GetNumBroadPhaseLayers() const override
    {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    [[nodiscard]] JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
    {
        JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
        return mObjectToBroadPhase[inLayer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
    {
        switch ((JPH::BroadPhaseLayer::Type)inLayer) {
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:
            return "NON_MOVING";
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:
            return "MOVING";
        default:
            JPH_ASSERT(false);
            return "INVALID";
        }
    }
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

private:
    JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS] {};
};

/// Class that determines if an object layer can collide with a broadphase layer
class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
    {
        switch (inLayer1) {
        case Layers::NON_MOVING:
            return inLayer2 == BroadPhaseLayers::MOVING;
        case Layers::MOVING:
            return true;
        default:
            JPH_ASSERT(false);
            return false;
        }
    }
};

// An example contact listener
class MyContactListener : public JPH::ContactListener {
public:
    // See: ContactListener
    JPH::ValidateResult OnContactValidate(const JPH::Body& inBody1, const JPH::Body& inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult& inCollisionResult) override
    {
        return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override
    {
    }

    void OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override
    {
    }

    void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override
    {
        // cout << "A contact was removed" << endl;
    }
};

// An example activation listener
class MyBodyActivationListener : public JPH::BodyActivationListener {
public:
    void OnBodyActivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) override
    {
        // cout << "A body got activated" << endl;
    }

    void OnBodyDeactivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) override
    {
        // cout << "A body went to sleep" << endl;
    }
};

// class ArenaTempAllocator : public JPH::TempAllocator {
// public:
//     explicit ArenaTempAllocator(Arena* arena)
//     {
//         this->arena = arena;
//     }
//
//     void* Allocate(JPH::uint inSize) override
//     {
//         printf("Called Alloc\n");
//
//         return arena_push(arena, inSize, JPH_RVECTOR_ALIGNMENT);
//     }
//
//     void Free(void* inAddress, JPH::uint inSize) override
//     {
//         printf("Called free\n");
//
//         if (!inAddress) {
//             assert(inSize == 0);
//         }
//
//         arena_pop(arena, inSize);
//     }
//
// private:
//     Arena* arena;
// };

struct PhysicsWorld {
    JPH::TempAllocatorImpl temp_allocator;
    JPH::JobSystemSingleThreaded job_system;
    BPLayerInterfaceImpl broad_phase_layer_interface;
    ObjectVsBroadPhaseLayerFilterImpl object_vs_broadphase_layer_filter;
    ObjectLayerPairFilterImpl object_vs_object_layer_filter;
    MyContactListener contact_listener;
    MyBodyActivationListener body_activation_listener;
    JPH::PhysicsSystem physics_system;
};

struct CharacterController {
    JPH::CharacterVirtual character_virtual;
};

PhysicsWorld* create_physics_world(Arena& arena)
{
    auto* world = arena.Push<PhysicsWorld>();

    JPH::RegisterDefaultAllocator();

    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    new (&world->temp_allocator) JPH::TempAllocatorImpl(200 * 1024 * 1024);

    new (&world->job_system) JPH::JobSystemSingleThreaded;

    const uint cMaxBodies = 65536;
    const uint cNumBodyMutexes = 0;
    const uint cMaxBodyPairs = 65536;
    const uint cMaxContactConstraints = 10240;

    // Create mapping table from object layer to broadphase layer
    // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
    // Also have a look at BroadPhaseLayerInterfaceTable or BroadPhaseLayerInterfaceMask for a simpler interface.
    new (&world->broad_phase_layer_interface) BPLayerInterfaceImpl;

    // Create class that filters object vs broadphase layers
    // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
    // Also have a look at ObjectVsBroadPhaseLayerFilterTable or ObjectVsBroadPhaseLayerFilterMask for a simpler interface.
    new (&world->object_vs_broadphase_layer_filter) ObjectVsBroadPhaseLayerFilterImpl;

    // Create class that filters object vs object layers
    // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
    // Also have a look at ObjectLayerPairFilterTable or ObjectLayerPairFilterMask for a simpler interface.
    new (&world->object_vs_object_layer_filter) ObjectLayerPairFilterImpl;

    new (&world->contact_listener) MyContactListener;
    new (&world->body_activation_listener) MyBodyActivationListener;

    new (&world->physics_system) JPH::PhysicsSystem;
    world->physics_system.Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints, world->broad_phase_layer_interface, world->object_vs_broadphase_layer_filter, world->object_vs_object_layer_filter);

    world->physics_system.SetContactListener(&world->contact_listener);
    world->physics_system.SetBodyActivationListener(&world->body_activation_listener);

    return world;
}

CharacterController* create_character_controller(PhysicsWorld* physics_world, CharacterControllerCreateInfo* create_info, Arena& arena)
{
    auto* controller = arena.Push<CharacterController>();

    JPH::CharacterVirtualSettings character_settings;
    character_settings.mMass = create_info->mass;
    character_settings.mMaxStrength = create_info->max_strength;

    character_settings.mShape = JPH::CapsuleShapeSettings(0.4f, 0.3f).Create().Get();

    // Manually call the constructor
    new (&controller->character_virtual) JPH::CharacterVirtual(&character_settings, JPH::RVec3(0, 2, 0), JPH::Quat::sIdentity(), &physics_world->physics_system);

    return controller;
}

static glm::vec3 to_glm(JPH::Vec3 vec3) {
    return {vec3.GetX(), vec3.GetY(), vec3.GetZ()};
}

static JPH::Vec3 to_jph_vec3(glm::vec3 vector3) {
    return {vector3.x, vector3.y, vector3.z};
}

glm::vec3 character_get_linear_velocity(CharacterController* character)
{
    return to_glm(character->character_virtual.GetLinearVelocity());
}

void character_set_linear_velocity(CharacterController* character, glm::vec3 velocity)
{
    character->character_virtual.SetLinearVelocity(to_jph_vec3(velocity));
}

glm::vec3 character_get_position(CharacterController* character)
{
    return to_glm(character->character_virtual.GetPosition());
}

void character_set_position(CharacterController* character, glm::vec3 position)
{
    character->character_virtual.SetPosition(to_jph_vec3(position));
}

bool character_is_grounded(CharacterController* character)
{
    return character->character_virtual.GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;
}

void character_update(PhysicsWorld* physics_world, CharacterController* character, float dt, glm::vec3 gravity)
{
    JPH::Vec3 jph_gravity(gravity.x, gravity.y, gravity.z);

    JPH::CharacterVirtual::ExtendedUpdateSettings settings;

    character->character_virtual.ExtendedUpdate(
        dt,
        jph_gravity,
        settings,
        JPH::BroadPhaseLayerFilter(), // change me
        JPH::ObjectLayerFilter(), // change me
        JPH::BodyFilter(),
        JPH::ShapeFilter(),
        physics_world->temp_allocator);
}

BodyID create_convex_hull_static_collider(PhysicsWorld* physics_world, MeshData* mesh, Arena& arena)
{
    // TempMemory temp = arena.begin_temp_memory();
    //
    auto* vertices = arena.PushArray<JPH::Vec3>(mesh->vertices_count);

    for (size_t i = 0; i < mesh->vertices_count; i++) {
        Vertex* vertex = &mesh->vertices[i];
        vertices[i] = to_jph_vec3(vertex->position);
    }

    JPH::ConvexHullShapeSettings settings(vertices, (int)mesh->vertices_count);

    auto result = settings.Create();

    if (result.HasError()) {
        printf("%s\n", result.GetError().c_str());
        return JPH::BodyID::cInvalidBodyID;
    }

    JPH::BodyCreationSettings body_settings(
        result.Get(),
        JPH::RVec3(0, 0, 0),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Static,
        Layers::NON_MOVING);

    JPH::BodyInterface& body_interface = physics_world->physics_system.GetBodyInterface();

    // arena.end_temp_memory(temp);
    //
    return body_interface.CreateAndAddBody(body_settings, JPH::EActivation::Activate).GetIndexAndSequenceNumber();
}

void update_physics_world(PhysicsWorld* physics_world, float dt, Arena& temp_arena)
{
    // // FIXME: Actually use it
    // ArenaTempAllocator temp_allocator(temp_arena);

    // FIXME: Handle error
    physics_world->physics_system.Update(dt, 1, &physics_world->temp_allocator, &physics_world->job_system);
}
