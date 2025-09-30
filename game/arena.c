#include "arena.h"

#include <assert.h>
#include <stdbool.h>

#define ALIGN_TO(n, alignment) (((n) + (alignment) - 1) & ~((alignment) - 1))

Arena create_arena(void* base, size_t size)
{
    Arena arena = {};
    arena.base = base;
    arena.current = base;
    arena.size = size;

    return arena;
}

void* arena_push(Arena* arena, size_t size, int alignment)
{
    uintptr_t aligned = ALIGN_TO((uintptr_t)arena->current, alignment);

    if (aligned + size > (uintptr_t)arena->base + arena->size) {
        return NULL;
    }

    void* result = (void*)aligned;
    arena->current = (uint8_t*)(aligned + size);
    return result;
}

void arena_pop(Arena* arena, size_t size)
{
    if (arena->current - size < arena->base) {
        assert(false);
        return;
    }

    arena->current -= size;
}

void* arena_push_zero(Arena* arena, size_t size, int alignment)
{
    void* ptr = arena_push(arena, size, alignment);
    if (ptr == NULL)
        return NULL;
    memset(ptr, 0, size);
    return ptr;
}
void* arena_realloc(Arena* arena, void* ptr, size_t old_size, size_t new_size, int alignment)
{
    uint8_t* u8_ptr = ptr;

    if (new_size <= old_size) {
        return ptr;
    }

    if (u8_ptr + old_size == arena->current) {
        arena->current += new_size - old_size;
        return ptr;
    }

    void* new_ptr = arena_push(arena, new_size, alignment);
    memcpy(new_ptr, ptr, old_size);

    return new_ptr;
}

void arena_clear(Arena* arena)
{
    arena->current = arena->base;
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
