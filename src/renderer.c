#include "renderer.h"

#include "logger.h"

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>

typedef struct {
    mat4 proj_view_matrix;
    mat4 model_matrix;
} VertexUniforms;

static SDL_GPUShader* load_shader(SDL_GPUDevice* device, const char* path, SDL_GPUShaderStage stage,
    int num_uniform_buffers, int num_samplers)
{
    size_t shader_code_size;
    void* shader_code = SDL_LoadFile(path, &shader_code_size);

    if (!shader_code)
        return NULL;

    SDL_GPUShaderCreateInfo create_info = {};
    create_info.code = (Uint8*)shader_code;
    create_info.code_size = shader_code_size;
    create_info.entrypoint = "main";
    create_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
    create_info.stage = stage;
    create_info.num_samplers = num_samplers;
    create_info.num_storage_buffers = 0;
    create_info.num_storage_textures = 0;
    create_info.num_uniform_buffers = num_uniform_buffers;

    SDL_GPUShader* shader = SDL_CreateGPUShader(device, &create_info);

    SDL_free(shader_code);

    return shader;
}

bool renderer_init(Renderer* renderer, SDL_Window* window)
{
    renderer->window = window;

    renderer->device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, "vulkan");

    if (!SDL_ClaimWindowForGPUDevice(renderer->device, renderer->window)) {
        log_err("%s", SDL_GetError());
    }

    // bool supports_mailbox = SDL_WindowSupportsGPUPresentMode(renderer->device, renderer->window, SDL_GPU_PRESENTMODE_MAILBOX);
    SDL_SetGPUSwapchainParameters(renderer->device, renderer->window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);

    SDL_GPUShader* vertex_shader = load_shader(renderer->device, "shaders/vert.spv", SDL_GPU_SHADERSTAGE_VERTEX, 1, 0);
    SDL_GPUShader* fragment_shader = load_shader(renderer->device, "shaders/frag.spv", SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 1);

    SDL_GPUGraphicsPipelineCreateInfo pipeline_create_info = {};
    pipeline_create_info.vertex_shader = vertex_shader;
    pipeline_create_info.fragment_shader = fragment_shader;
    pipeline_create_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipeline_create_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_LINE;

    SDL_GPUVertexBufferDescription vertex_buffer_description[1] = {};
    vertex_buffer_description[0].slot = 0;
    vertex_buffer_description[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vertex_buffer_description[0].instance_step_rate = 0;
    vertex_buffer_description[0].pitch = sizeof(Vertex);

    pipeline_create_info.vertex_input_state.num_vertex_buffers = 1;
    pipeline_create_info.vertex_input_state.vertex_buffer_descriptions = vertex_buffer_description;

    SDL_GPUVertexAttribute vertex_attributes[2] = {};
    vertex_attributes[0].buffer_slot = 0;
    vertex_attributes[0].location = 0;
    vertex_attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertex_attributes[0].offset = offsetof(Vertex, position);

    // UV attribute
    vertex_attributes[1].buffer_slot = 0;
    vertex_attributes[1].location = 1;
    vertex_attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    vertex_attributes[1].offset = offsetof(Vertex, texcoords);

    pipeline_create_info.vertex_input_state.num_vertex_attributes = 2;
    pipeline_create_info.vertex_input_state.vertex_attributes = vertex_attributes;

    SDL_GPUColorTargetDescription color_target_descriptions[1] = {};
    color_target_descriptions[0].format = SDL_GetGPUSwapchainTextureFormat(renderer->device, window);

    pipeline_create_info.target_info.num_color_targets = 1;
    pipeline_create_info.target_info.color_target_descriptions = color_target_descriptions;
    pipeline_create_info.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    pipeline_create_info.target_info.has_depth_stencil_target = true;

    pipeline_create_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;

    pipeline_create_info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_4;

    SDL_GPUDepthStencilState depth_stencil_state = {
        .compare_op = SDL_GPU_COMPAREOP_LESS,
        .back_stencil_state = {
            .compare_op = SDL_GPU_COMPAREOP_ALWAYS },
        .front_stencil_state = { .compare_op = SDL_GPU_COMPAREOP_ALWAYS },
        .compare_mask = 0,
        .write_mask = 0,
        .enable_depth_test = true,
        .enable_depth_write = true,
        .enable_stencil_test = false
    };

    pipeline_create_info.depth_stencil_state = depth_stencil_state;

    renderer->graphics_pipeline = SDL_CreateGPUGraphicsPipeline(renderer->device, &pipeline_create_info);

    if (!renderer->graphics_pipeline) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not create graphics pipeline: %s\n", SDL_GetError());
        return false;
    }

    SDL_ReleaseGPUShader(renderer->device, vertex_shader);
    SDL_ReleaseGPUShader(renderer->device, fragment_shader);

    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    SDL_GPUTextureCreateInfo depth_texture_create_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        .width = width,
        .height = height,
        .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
        .sample_count = SDL_GPU_SAMPLECOUNT_4,
    };

    renderer->depth_texture = SDL_CreateGPUTexture(renderer->device, &depth_texture_create_info);

    if (!renderer->depth_texture) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to create depth texture: %s", SDL_GetError());
    }

    SDL_GPUTextureCreateInfo msaa_texture_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GetGPUSwapchainTextureFormat(renderer->device, renderer->window),
        .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
        .width = width,
        .height = height,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .sample_count = SDL_GPU_SAMPLECOUNT_4,
    };
    renderer->msaa_texture = SDL_CreateGPUTexture(renderer->device, &msaa_texture_info);

    SDL_GPUSamplerCreateInfo sampler_info = {
        .min_filter = SDL_GPU_FILTER_NEAREST,
        .mag_filter = SDL_GPU_FILTER_NEAREST,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    };

    renderer->texture_sampler = SDL_CreateGPUSampler(renderer->device, &sampler_info);

    renderer->mesh_storage.capacity = 1024 * 10;
    renderer->mesh_storage.meshes = SDL_malloc(renderer->mesh_storage.capacity);

    renderer->texture_storage.capacity = 1024 * 10;
    renderer->texture_storage.textures = SDL_malloc(renderer->texture_storage.capacity);

    glm_perspective(glm_rad(45.0f), (float)width / (float)height, 1.0f, 4096.0f, renderer->projection_matrix);

    return true;
}

MeshHandle renderer_create_mesh(Renderer* renderer, MeshData* mesh_data)
{
    if (renderer->mesh_storage.capacity <= renderer->mesh_storage.count) {
        log_err("Mesh storage full");
        return 0;
    }

    if (mesh_data->indices_count == 0 || mesh_data->vertices_count == 0) {
        log_err("Empty mesh");
        return 0;
    }

    size_t vertices_size = mesh_data->vertices_count * sizeof(*mesh_data->vertices);
    size_t indices_size = mesh_data->indices_count * sizeof(*mesh_data->indices);

    SDL_GPUTransferBufferCreateInfo transfer_info = {
        .size = vertices_size + indices_size,
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
    };

    SDL_GPUTransferBuffer* transfer_buffer = SDL_CreateGPUTransferBuffer(renderer->device, &transfer_info);
    if (!transfer_buffer) {
        log_err("%s", SDL_GetError());
        return 0;
    }

    uint8_t* transfer_data = SDL_MapGPUTransferBuffer(renderer->device, transfer_buffer, false);
    if (!transfer_data) {
        log_err("%s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(renderer->device, transfer_buffer);
        return 0;
    }

    memcpy(transfer_data, mesh_data->vertices, vertices_size);
    memcpy(transfer_data + vertices_size, mesh_data->indices, indices_size);

    SDL_UnmapGPUTransferBuffer(renderer->device, transfer_buffer);

    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(renderer->device);
    if (!command_buffer) {
        log_err("%s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(renderer->device, transfer_buffer);
        return 0;
    }

    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    if (!copy_pass) {
        log_err("%s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(renderer->device, transfer_buffer);
        return 0;
    }

    SDL_GPUBufferCreateInfo vertex_buffer_create_info = {
        .size = vertices_size,
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
    };

    SDL_GPUBuffer* vertex_buffer = SDL_CreateGPUBuffer(renderer->device, &vertex_buffer_create_info);
    if (!vertex_buffer) {
        log_err("%s", SDL_GetError());
        SDL_EndGPUCopyPass(copy_pass);
        SDL_ReleaseGPUTransferBuffer(renderer->device, transfer_buffer);
        return 0;
    }

    SDL_GPUTransferBufferLocation vertex_buffer_location = {
        .transfer_buffer = transfer_buffer,
        .offset = 0,
    };

    SDL_GPUBufferRegion vertex_buffer_region = {
        .buffer = vertex_buffer,
        .size = vertices_size,
        .offset = 0,
    };

    SDL_UploadToGPUBuffer(copy_pass, &vertex_buffer_location, &vertex_buffer_region, true);

    SDL_GPUBufferCreateInfo index_buffer_create_info = {
        .size = indices_size,
        .usage = SDL_GPU_BUFFERUSAGE_INDEX,
    };

    SDL_GPUBuffer* index_buffer = SDL_CreateGPUBuffer(renderer->device, &index_buffer_create_info);
    if (!index_buffer) {
        log_err("%s", SDL_GetError());
        SDL_ReleaseGPUBuffer(renderer->device, vertex_buffer);
        SDL_EndGPUCopyPass(copy_pass);
        SDL_ReleaseGPUTransferBuffer(renderer->device, transfer_buffer);
        return 0;
    }

    SDL_GPUTransferBufferLocation index_buffer_location = {
        .transfer_buffer = transfer_buffer,
        .offset = vertices_size,
    };

    SDL_GPUBufferRegion index_buffer_region = {
        .buffer = index_buffer,
        .size = indices_size,
        .offset = 0,
    };

    SDL_UploadToGPUBuffer(copy_pass, &index_buffer_location, &index_buffer_region, true);

    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(command_buffer);

    SDL_ReleaseGPUTransferBuffer(renderer->device, transfer_buffer);

    MeshResource* mesh_resource = &renderer->mesh_storage.meshes[renderer->mesh_storage.count++];
    mesh_resource->handle = renderer->mesh_storage.count;
    mesh_resource->vertex_buffer = vertex_buffer;
    mesh_resource->index_buffer = index_buffer;
    mesh_resource->indices_count = mesh_data->indices_count;

    return mesh_resource->handle;
}
TextureHandle renderer_create_texture(Renderer* renderer, const uint8_t* rgba_data, int width, int height)
{
    SDL_GPUTextureCreateInfo texture_create_info = {};
    texture_create_info.type = SDL_GPU_TEXTURETYPE_2D;
    texture_create_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texture_create_info.width = width;
    texture_create_info.height = height;
    texture_create_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    texture_create_info.layer_count_or_depth = 1;
    texture_create_info.num_levels = 1;
    texture_create_info.sample_count = SDL_GPU_SAMPLECOUNT_1;

    SDL_GPUTexture* texture = SDL_CreateGPUTexture(renderer->device, &texture_create_info);
    if (!texture) {
        log_err("%", SDL_GetError());
        return 0;
    }

    SDL_GPUTransferBufferCreateInfo transfer_buffer_create_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = width * height * 4,
    };

    SDL_GPUTransferBuffer* transfer_buffer = SDL_CreateGPUTransferBuffer(renderer->device, &transfer_buffer_create_info);
    if (!transfer_buffer) {
        SDL_ReleaseGPUTexture(renderer->device, texture);
        return 0;
    }

    void* transfer_data = SDL_MapGPUTransferBuffer(renderer->device, transfer_buffer, true);

    if (!transfer_data) {
        SDL_ReleaseGPUTransferBuffer(renderer->device, transfer_buffer);
        SDL_ReleaseGPUTexture(renderer->device, texture);
        return 0;
    }

    memcpy(transfer_data, rgba_data, width * height * 4);

    SDL_UnmapGPUTransferBuffer(renderer->device, transfer_buffer);

    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(renderer->device);
    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTextureTransferInfo transfer_info = {
        .offset = 0,
        .pixels_per_row = width,
        .rows_per_layer = height,
        .transfer_buffer = transfer_buffer,
    };

    SDL_GPUTextureRegion region = {
        .w = width,
        .h = height,
        .d = 1,
        .texture = texture,
    };

    SDL_UploadToGPUTexture(copy_pass, &transfer_info, &region, true);

    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(cmd);

    SDL_ReleaseGPUTransferBuffer(renderer->device, transfer_buffer);

    TextureResource* texture_resource = &renderer->texture_storage.textures[renderer->texture_storage.count++];
    texture_resource->handle = renderer->texture_storage.count;
    texture_resource->texture = texture;

    return texture_resource->handle;
}

void renderer_play_draw_list(Renderer* renderer, DrawList* draw_list)
{
    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(renderer->device);

    SDL_GPUTexture* swapchain_texture;
    Uint32 width, height;
    SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, renderer->window, &swapchain_texture, &width, &height);

    if (!swapchain_texture) {
        SDL_SubmitGPUCommandBuffer(command_buffer);
        return;
    }

    SDL_FColor clear_color = {
        draw_list->clear_color.r,
        draw_list->clear_color.g,
        draw_list->clear_color.b,
        draw_list->clear_color.a,
    };

    SDL_GPUColorTargetInfo color_target_info = {
        .texture = renderer->msaa_texture,
        .mip_level = 0,
        .layer_or_depth_plane = 0,
        .clear_color = clear_color,
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_RESOLVE,
        .resolve_texture = swapchain_texture,
        .cycle = true,
    };

    SDL_GPUDepthStencilTargetInfo depth_info = {
        .texture = renderer->depth_texture,
        .clear_depth = 1.0f,
        .clear_stencil = 0,
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_DONT_CARE,
        .stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
        .stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
        .cycle = true
    };

    SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buffer, &color_target_info, 1, &depth_info);

    VertexUniforms vertex_uniforms;

    mat4 view_matrix;
    vec3 eye, center;
    glm_vec3_make(draw_list->camera.position.items, eye);
    glm_vec3_make(draw_list->camera.target.items, center);

    glm_lookat(eye, center, (vec3) { 0.0f, 1.0f, 0.0f }, view_matrix);

    glm_mul(renderer->projection_matrix, view_matrix, vertex_uniforms.proj_view_matrix);

    SDL_BindGPUGraphicsPipeline(render_pass, renderer->graphics_pipeline);

    for (size_t i = 0; i < draw_list->count; i++) {
        DrawCommand* cmd = &draw_list->commands[i];

        switch (cmd->type) {
        case DRAW_MESH: {
            MeshHandle mesh_handle = cmd->as.draw_mesh.mesh;
            TextureHandle texture_handle = cmd->as.draw_mesh.texture;
            Transform transform = cmd->as.draw_mesh.transform;

            vec3 scale;
            glm_vec3_make(transform.scale.items, scale);

            vec4 rotation;
            glm_vec4_make(transform.rotation.items, rotation);

            vec3 translation;
            glm_vec3_copy(transform.position.items, translation);

            glm_mat4_identity(vertex_uniforms.model_matrix);
            glm_translate(vertex_uniforms.model_matrix, translation);
            glm_quat_rotate(vertex_uniforms.model_matrix, rotation, vertex_uniforms.model_matrix);
            glm_scale(vertex_uniforms.model_matrix, scale);

            if (mesh_handle == 0 || mesh_handle > renderer->mesh_storage.count) {
                log_err("DrawMesh: Invalid MeshHandle");
                continue;
            }

            MeshResource* mesh_resource = &renderer->mesh_storage.meshes[mesh_handle - 1];
            TextureResource* texture_resource = &renderer->texture_storage.textures[texture_handle - 1];

            SDL_GPUBufferBinding vertex_buffer_bindings = {
                .buffer = mesh_resource->vertex_buffer,
                .offset = 0,
            };

            SDL_BindGPUVertexBuffers(render_pass, 0, &vertex_buffer_bindings, 1);

            SDL_GPUBufferBinding index_buffer_bindings = {
                .buffer = mesh_resource->index_buffer,
                .offset = 0,
            };

            SDL_BindGPUIndexBuffer(render_pass, &index_buffer_bindings, SDL_GPU_INDEXELEMENTSIZE_16BIT);

            SDL_PushGPUVertexUniformData(command_buffer, 0, &vertex_uniforms, sizeof(vertex_uniforms));

            SDL_GPUTextureSamplerBinding binding = {
                .texture = texture_resource->texture,
                .sampler = renderer->texture_sampler,
            };

            SDL_BindGPUFragmentSamplers(render_pass, 0, &binding, 1);

            SDL_DrawGPUIndexedPrimitives(render_pass, mesh_resource->indices_count, 1, 0, 0, 0);
        } break;
        }
    }

    SDL_EndGPURenderPass(render_pass);
    SDL_SubmitGPUCommandBuffer(command_buffer);
}
