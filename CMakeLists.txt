cmake_minimum_required(VERSION 3.30)
project(Almond C)

set(CMAKE_C_STANDARD 11)

find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)

add_compile_options(
        -Wall
        -Wextra
)

add_executable(almond
        src/main.c
        src/logger.c
        src/renderer.c
        src/arena_linux.c
        src/public/almond.h
        src/file_watcher.c
)

add_library(game SHARED
        game/game.c
        game/texture.c
        game/map.c
        game/geometry.c
        game/geometry.h
        game/arena.c
        game/render_commands.c
)

target_include_directories(almond PRIVATE vendor)
target_include_directories(game PRIVATE src/public vendor)

target_link_libraries(almond PRIVATE SDL3::SDL3 m)
