cmake_minimum_required(VERSION 3.30)
project(Almond CXX)

include(FetchContent)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Distribution")

set(DOUBLE_PRECISION OFF)

set(GENERATE_DEBUG_SYMBOLS ON)

set(OVERRIDE_CXX_FLAGS ON)

set(CROSS_PLATFORM_DETERMINISTIC OFF)

set(INTERPROCEDURAL_OPTIMIZATION ON)
set(FLOATING_POINT_EXCEPTIONS_ENABLED OFF)
set(CPP_EXCEPTIONS_ENABLED OFF)
set(CPP_RTTI_ENABLED OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Number of bits to use in ObjectLayer. Can be 16 or 32.
set(OBJECT_LAYER_BITS 16)

# Include Jolt
FetchContent_Declare(
        JoltPhysics
        GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics
        GIT_TAG v5.3.0
        SOURCE_SUBDIR Build
)
FetchContent_MakeAvailable(JoltPhysics)

FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_BRANCH main
        GIT_TAG release-3.2.22
)

FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1
)

FetchContent_MakeAvailable(glm)

FetchContent_MakeAvailable(SDL3)

set(CMAKE_CXX_STANDARD 17)

add_compile_options(
        -Wall
        -Wextra
)

add_executable(almond
        src/main.cpp
        src/logger.cpp
        src/renderer.cpp
        src/public/almond.h
        src/file_watcher.cpp
)

add_library(game SHARED
        game/game.cpp
        game/texture.cpp
        game/map.cpp
        game/geometry.cpp
        game/geometry.h
        game/arena.cpp
        game/render_commands.cpp
        game/physics.cpp
        game/physics.h
        game/gltf_loader.cpp
        game/gltf_loader.h
        game/shape.cpp
)

target_include_directories(almond PRIVATE vendor)

target_include_directories(game PUBLIC ${JoltPhysics_SOURCE_DIR}/..)
target_include_directories(game PRIVATE src/public vendor)
target_link_libraries(game PRIVATE Jolt glm::glm)

target_link_libraries(almond PRIVATE SDL3::SDL3 m glm::glm)
