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
    SDL_GPUDevice* device;
    SDL_GPUGraphicsPipeline* graphics_pipeline;
    SDL_GPUTexture* depth_texture;
    SDL_Window* window;

    MeshStorage mesh_storage;

    mat4 projection_matrix;
} Renderer;

bool renderer_init(Renderer* renderer, SDL_Window* window);
MeshHandle renderer_create_mesh(Renderer* renderer, MeshData* mesh_data);
void renderer_play_draw_list(Renderer* renderer, DrawList* draw_list);
