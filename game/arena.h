#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define DEFAULT_ALIGNMENT 16

typedef struct {
    uint8_t* base;
    size_t current;
    size_t size;
} Arena;

typedef struct {
    size_t start;
    size_t *arena_current;
} TempMemory;

void* arena_push(Arena* arena, size_t size);
void* arena_push_zero(Arena* arena, size_t size);
TempMemory begin_temp_memory(Arena* arena);
void end_temp_memory(TempMemory temp);

#define PushArray(arena, type, count) (type *)arena_push((arena), sizeof(type)*(count))
#define PushArrayZero(arena, type, count) (type *)arena_push_zero((arena), sizeof(type)*(count))
#define PushStruct(arena, type) PushArray((arena), type, 1)
#define PushStructZero(arena, type) PushArrayZero((arena), type, 1)

