#include "file_watcher.h"
#include "logger.h"
#include "public/almond.h"
#include "renderer.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_loadso.h>
#include <libgen.h>
#include <cstdio>
#include <cstdlib>
#include <sys/mman.h>

Renderer renderer;

#define Megabytes(n) (n * 1024 * 1024)
#define Gigabytes(n) (n * 1024 * 1024 * 1024)

LOAD_ENTIRE_FILE(load_entire_file_sdl)
{
    return SDL_LoadFile(file, datasize);
}

CREATE_TEXTURE(create_texture_sdl)
{
    return renderer_create_texture(&renderer, rgba_data, width, height);
}

CREATE_MESH(create_mesh_sdl)
{
    return renderer_create_mesh(&renderer, mesh_data);
}

static Api api = {
    .load_entire_file = load_entire_file_sdl,
    .create_texture = create_texture_sdl,
    .create_mesh = create_mesh_sdl,
};

typedef struct {
    SDL_Window* window;
    SDL_GPUDevice* device;
    SDL_SharedObject* game_so;
    GameIterateFn* game_iterate;
    const char* game_so_file;
} PlatformState;

void reload_game_so(PlatformState* platform_state, const char* path)
{
    if (platform_state->game_so) {
        SDL_UnloadObject(platform_state->game_so);
    }

    platform_state->game_so = SDL_LoadObject(path);
    if (!platform_state->game_so) {
        log_fatal("%s", SDL_GetError());
    }

    platform_state->game_iterate = (GameIterateFn*)SDL_LoadFunction(platform_state->game_so, "game_iterate");
    if (!platform_state->game_iterate) {
        log_fatal("%s", SDL_GetError());
    }
}

typedef struct {
    PlatformState* platform_state;
    const char* game_so_name;
    const char* game_so_path;
} GameSoWatcherCallbackData;

void game_so_watcher_callback(FileWatcherEvent* event, void* user_data)
{
    auto* data = (GameSoWatcherCallbackData*)user_data;

    if (event->type == FW_MODIFY && strcmp(event->file_name, data->game_so_name) == 0) {
        reload_game_so(data->platform_state, data->game_so_path);
        log_info("Reloaded game SO");
    }
}

int main(int argc, char* argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: almond ./libgame.so\n");
        exit(1);
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        log_fatal("%s", SDL_GetError());
    }

    PlatformState platform = {};

    platform.window = SDL_CreateWindow("Game", 1280, 720, 0);

    if (!platform.window) {
        log_fatal("%s", SDL_GetError());
    }

    GameMemory memory = {};

    memory.permanent_storage_size = Megabytes(200);
    memory.permanent_storage = mmap(NULL, memory.permanent_storage_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ;

    memory.transient_storage_size = Gigabytes(1);
    memory.transient_storage = mmap(NULL, memory.transient_storage_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ;

    renderer_init(&renderer, platform.window);

    DrawList draw_list = {};
    draw_list.capacity = Megabytes(10);
    draw_list.commands = (DrawCommand*)SDL_malloc(draw_list.capacity);

    reload_game_so(&platform, argv[1]);

    char* dir_path = SDL_strdup(argv[1]);
    dir_path = dirname(dir_path);

    char* file_name = SDL_strdup(argv[1]);
    file_name = basename(file_name);

    GameSoWatcherCallbackData callback_data = {};
    callback_data.platform_state = &platform;
    callback_data.game_so_name = file_name;
    callback_data.game_so_path = argv[1];

    FileWatcher* file_watcher = create_file_watcher(dir_path, game_so_watcher_callback, &callback_data);
    if (!file_watcher) {
        log_fatal("Could not create file watcher for game SO");
    }

    SDL_free(dir_path);

    Uint64 last_ticks = SDL_GetTicks();

    bool running = true;
    ControllerInput input = {};

    while (running) {
        Uint64 current_ticks = SDL_GetTicks();
        float dt = (float)(current_ticks - last_ticks) / 1000.0f;

        // Scoobydoo hack to iterate over all buttons
        for (size_t i = 0; i < sizeof(input.buttons) / sizeof(GameButtonState); i++) {
            input.buttons[i].half_transition_count = 0;
        }

        input.mouse_movement = glm::vec2(0.0f);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_WINDOW_MOUSE_ENTER: {
                SDL_SetWindowRelativeMouseMode(platform.window, true);
            } break;
            case SDL_EVENT_MOUSE_MOTION: {
                input.mouse_movement.x += event.motion.xrel;
                input.mouse_movement.y += event.motion.yrel;
            } break;
            case SDL_EVENT_KEY_DOWN: {
                if (event.key.repeat) continue;

                switch (event.key.key) {
                case SDLK_ESCAPE: {
                    SDL_SetWindowRelativeMouseMode(platform.window, false);
                } break;
                case SDLK_W: {
                    input.move_up.half_transition_count++;
                    input.move_up.pressed = true;
                } break;
                case SDLK_A: {
                    input.move_left.half_transition_count++;
                    input.move_left.pressed = true;
                } break;
                case SDLK_S: {
                    input.move_down.half_transition_count++;
                    input.move_down.pressed = true;
                } break;
                case SDLK_D: {
                    input.move_right.half_transition_count++;
                    input.move_right.pressed = true;
                } break;
                case SDLK_SPACE: {
                    input.move_jump.half_transition_count++;
                    input.move_jump.pressed = true;
                } break;
                default:
                    break;
                }
            } break;
            case SDL_EVENT_KEY_UP: {
                if (event.key.repeat) continue;;

                switch (event.key.key) {
                case SDLK_W: {
                    input.move_up.half_transition_count++;
                    input.move_up.pressed = false;
                } break;
                case SDLK_A: {
                    input.move_left.half_transition_count++;
                    input.move_left.pressed = false;
                } break;
                case SDLK_S: {
                    input.move_down.half_transition_count++;
                    input.move_down.pressed = false;
                } break;
                case SDLK_D: {
                    input.move_right.half_transition_count++;
                    input.move_right.pressed = false;
                } break;
                case SDLK_SPACE: {
                    input.move_jump.half_transition_count++;
                    input.move_jump.pressed = false;
                } break;
                default:
                    break;
                }
            } break;
            default:
                break;
            }
        }

        file_watcher_update(file_watcher);

        draw_list.count = 0;

        platform.game_iterate(&memory, &input, &draw_list, dt, &api);

        renderer_play_draw_list(&renderer, &draw_list);

        last_ticks = current_ticks;
    }

    SDL_DestroyWindow(platform.window);
}