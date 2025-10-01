#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Lightweight handles
template<typename Tag>
struct Handle {
    uint32_t value;

    explicit constexpr Handle(uint32_t v = 0) : value(v) {}

    static constexpr Handle invalid() { return Handle(0); }
    [[nodiscard]] constexpr bool is_valid() const { return value != 0; }
    constexpr explicit operator bool() const { return is_valid(); }

    constexpr bool operator==(Handle o) const { return value == o.value; }
    constexpr bool operator!=(Handle o) const { return value != o.value; }
};

struct MeshTag {};
struct TextureTag {};
struct MaterialTag {};

using MeshHandle = Handle<MeshTag>;
using TextureHandle = Handle<TextureTag>;
using MaterialHandle = Handle<MaterialTag>;

struct Camera {
    glm::vec3 position;
    glm::vec3 target;
};

struct Vertex {
    glm::vec3 position;
    glm::vec2 texcoords;
};

struct MeshData {
    Vertex* vertices;
    size_t vertices_count;

    uint16_t* indices;
    size_t indices_count;
};

typedef enum {
    TODO
} MaterialFlags;

#define LOAD_ENTIRE_FILE(name) void*(name)(const char* file, size_t* datasize)
typedef LOAD_ENTIRE_FILE(LoadEntireFileFn);

#define CREATE_MESH(name) MeshHandle(name)(MeshData * mesh_data)
typedef CREATE_MESH(CreateMeshFn);

#define CREATE_TEXTURE(name) TextureHandle(name)(const uint8_t* rgba_data, int width, int height)
typedef CREATE_TEXTURE(CreateTextureFn);

#define CREATE_MATERIAL(name) MaterialHandle(name)(TextureHandle albedo, MaterialFlags flags)
typedef CREATE_MATERIAL(CreateMaterialFn);

struct Api {
    LoadEntireFileFn* load_entire_file;
    CreateMeshFn* create_mesh;
    CreateTextureFn* create_texture;
    CreateMaterialFn* create_material;
};

struct Transform {
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
};

enum class DrawCommandType {
    Invalid,
    DrawMesh,
};

struct DrawCommand {
    DrawCommandType type = DrawCommandType::Invalid;
    union {
        struct {
            MeshHandle mesh;
            TextureHandle texture;
            Transform transform;
        } draw_mesh;
        struct {
            MeshHandle mesh;
            Transform transform;
        } debug_collider;
    } as;
};

struct DrawList {
    glm::vec4 clear_color;
    Camera camera;
    DrawCommand* commands;
    size_t count;
    size_t capacity;
};

struct GameMemory {
    bool is_initialized;

    size_t permanent_storage_size;
    void* permanent_storage;

    size_t transient_storage_size;
    void* transient_storage;
};

struct GameButtonState {
    int half_transition_count;
    bool pressed;
};

typedef struct {
    bool is_gamepad;

    glm::vec2 mouse_movement;
    glm::vec2 left_stick;
    glm::vec2 right_stick;

    union {
        GameButtonState buttons[5];
        struct {
            GameButtonState move_up;
            GameButtonState move_down;
            GameButtonState move_right;
            GameButtonState move_left;
            GameButtonState move_jump;
        };
    };

} ControllerInput;

#define GAME_ITERATE(name) void(name)(GameMemory * memory, ControllerInput* input, DrawList * draw_list, float dt, Api * api)
typedef GAME_ITERATE(GameIterateFn);
