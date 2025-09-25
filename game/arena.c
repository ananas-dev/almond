#include "arena.h"

void* arena_push(Arena* arena, size_t size)
{
    void* ptr = arena->base + arena->current;

    size_t aligned_size = ((size + DEFAULT_ALIGNMENT - 1) / DEFAULT_ALIGNMENT) * DEFAULT_ALIGNMENT;
    arena->current += aligned_size;

    return ptr;
}

void* arena_push_zero(Arena* arena, size_t size)
{
    void* ptr = arena_push(arena, size);
    memset(ptr, 0, size);
    return ptr;
}

TempMemory begin_temp_memory(Arena* arena)
{
    return (TempMemory) {
        .arena_current = &arena->current,
        .start = arena->current
    };
}

void end_temp_memory(TempMemory temp)
{
    *temp.arena_current = temp.start;
}
