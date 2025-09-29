#pragma once

#include <stdint.h>
#include <string.h>

#define DEFAULT_ALIGNMENT 16

typedef struct {
    uint8_t* base;
    uint8_t* current;
    size_t size;
} Arena;

typedef struct {
    uint8_t* start;
    uint8_t** arena_current;
} TempMemory;

Arena create_arena(void* base, size_t size);
void* arena_push(Arena* arena, size_t size, int alignment);
void arena_pop(Arena* arena, size_t size);
void* arena_push_zero(Arena* arena, size_t size, int alignment);
void* arena_realloc(Arena* arena, void* ptr, size_t old_size, size_t new_size, int alignment);
void arena_clear(Arena* arena);
TempMemory begin_temp_memory(Arena* arena);
void end_temp_memory(TempMemory temp);

#define PushArray(arena, type, count) (type *)arena_push((arena), sizeof(type)*(count), 16)
#define PushArrayZero(arena, type, count) (type *)arena_push_zero((arena), sizeof(type)*(count), 16)
#define PushStruct(arena, type) PushArray((arena), type, 1)
#define PushStructZero(arena, type) PushArrayZero((arena), type, 1)

