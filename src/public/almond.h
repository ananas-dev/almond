#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef float Vector2[2];

typedef struct {
    union {
        struct {
            float x, y, z;
        };
        float items[3];
    };
} Vector3;

typedef float Quaternion[4];

typedef struct {
    float r, g, b, a;
} Color;

typedef struct {
    const char* data;
    size_t count;
} StringView;

#define SV(s) (StringView) { s,  sizeof(s) - 1 }

// Lightweights handles
typedef uint32_t MeshHandle;
typedef uint32_t TextureHandle;
typedef uint32_t MaterialHandle;

typedef struct {
    Vector3 position;
    Vector3 target;
    float yaw;
    float pitch;
} Camera;

typedef struct {
    Vector3* vertices;
    size_t vertices_count;

    uint16_t* indices;
    size_t indices_count;
} MeshData;

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

typedef struct {
    LoadEntireFileFn* load_entire_file;
    CreateMeshFn* create_mesh;
    CreateTextureFn* create_texture;
    CreateMaterialFn* create_material;
} Api;

typedef struct {
    Vector3 position;
    Quaternion rotation;
    Vector3 scale;
} Transform;

#define TRANSFORM_IDENTITY_INIT { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f } }
#define TRANSFORM_IDENTITY ((Transform)TRANSFORM_IDENTITY_INIT)

typedef enum {
    DRAW_MESH,
} DrawCommandType;

typedef struct {
    DrawCommandType type;
    union {
        struct {
            MeshHandle mesh;
            Transform transform;
        } draw_mesh;
    } as;
} DrawCommand;

typedef struct {
    Color clear_color;
    Camera camera;
    DrawCommand* commands;
    size_t count;
    size_t capacity;
} DrawList;

typedef struct {
    bool is_initialized;

    size_t permanent_storage_size;
    void* permanent_storage;

    size_t transient_storage_size;
    void* transient_storage;
} GameMemory;

typedef struct {
    int half_transition_count;
    bool pressed;
} GameButtonState;

typedef struct {
    bool is_gamepad;

    Vector2 mouse_movement;
    Vector2 left_stick;
    Vector2 right_stick;

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
