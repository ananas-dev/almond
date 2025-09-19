#pragma once

#include <stdint.h>
#include <stddef.h>

#define DEFAULT_ARENA_SIZE (16 * 1024 * 1024)

typedef struct
{
    uint8_t* base;
    size_t capacity;
    size_t size;
} Arena;

void arena_init(Arena* arena);
void* arena_push(Arena* arena, size_t size);
void* arena_push_zero(Arena* arena, size_t size);
void* arena_push_aligned(Arena* arena, size_t size, size_t alignment);
void arena_pop(Arena* arena, size_t size);
void arena_clean(Arena* arena);
size_t arena_checkpoint(Arena* arena);
void arena_restore_to_checkpoint(Arena* arena, size_t checkpoint);
void arena_free(Arena* arena);

#define PushArray(arena, type, count) (type *)arena_push((arena), sizeof(type)*(count))
#define PushArrayZero(arena, type, count) (type *)arena_push_zero((arena), sizeof(type)*(count))
#define PushArrayAligned(arena, type, count) (type *)arena_push_aligned((arena), sizeof(type)*(count), 16)
#define PushStruct(arena, type) PushArray((arena), (type), 1)
#define PushStructZero(arena, type) PushArrayZero((arena), (type), 1)
#define PushStructAligned(arena, type) PushArrayAligned((arena), (type), 1)
