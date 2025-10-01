#pragma once

#include "public/almond.h"
#include <SDL3/SDL_gpu.h>

#include <cglm/cglm.h>

typedef struct {
    MeshHandle handle;
    SDL_GPUBuffer* vertex_buffer;
    SDL_GPUBuffer* index_buffer;
    size_t indices_count;
} MeshResource;

typedef struct {
    MeshResource* meshes;
    size_t count;
    size_t capacity;
} MeshStorage;

typedef struct {
    TextureHandle handle;
    SDL_GPUTexture* texture;
} TextureResource;

typedef struct {
    TextureResource* textures;
    size_t count;
    size_t capacity;
} TextureStorage;

typedef struct {
    SDL_GPUDevice* device;
    SDL_Window* window;

    SDL_GPUTexture* depth_texture;
    SDL_GPUTexture* msaa_texture;

    SDL_GPUGraphicsPipeline* graphics_pipeline;
    SDL_GPUGraphicsPipeline* debug_collider_pipeline;

    MeshStorage mesh_storage;
    TextureStorage texture_storage;

    SDL_GPUSampler* texture_sampler;

    mat4 projection_matrix;
} Renderer;

bool renderer_init(Renderer* renderer, SDL_Window* window);
MeshHandle renderer_create_mesh(Renderer* renderer, MeshData* mesh_data);
TextureHandle renderer_create_texture(Renderer* renderer, const uint8_t* rgba_data, int width, int height);
void renderer_play_draw_list(Renderer* renderer, DrawList* draw_list);
