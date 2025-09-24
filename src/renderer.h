#pragma once

#include <SDL3/SDL.h>

typedef struct Renderer Renderer;

Renderer* create_renderer(SDL_Window* window);

