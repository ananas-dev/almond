#include "arena.h"

#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_stdinc.h>
#include <sys/mman.h>

void arena_init(Arena* arena)
{
    void* base = mmap(NULL, DEFAULT_ARENA_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    SDL_assert_release(base != MAP_FAILED);

    arena->base = base;
    arena->capacity = DEFAULT_ARENA_SIZE;
    arena->size = 0;
}

void* arena_push(Arena* arena, size_t size)
{
    size_t aligned_size = (size + 7) & ~7;

    if (arena->size + aligned_size > arena->capacity)
        return NULL;

    void* result = (uint8_t*)arena->base + arena->size;
    arena->size += aligned_size;

    return result;
}

void* arena_push_zero(Arena* arena, size_t size)
{
    void* result = arena_push(arena, size);
    SDL_memset(result, 0, size);
    return result;
}

void* arena_push_aligned(Arena* arena, size_t size, size_t alignment)
{
    // Ensure power of 2
    SDL_assert_release((alignment & (alignment - 1)) == 0);

    uintptr_t current_ptr = (uintptr_t)arena->base + arena->size;
    uintptr_t aligned_ptr = (current_ptr + (alignment - 1)) & ~(alignment - 1);

    size_t padding = aligned_ptr - current_ptr;
    size_t total_size = size + padding;

    if (arena->size + total_size > arena->capacity)
        return NULL;

    arena->size += total_size;
    return (void*)aligned_ptr;
}

void ArenaPop(Arena* arena, size_t size)
{
    if (size > arena->size)
    {
        arena->size = 0;
        return;
    }

    arena->size -= size;
}

void arena_clean(Arena* arena)
{
    arena->size = 0;
}

size_t arena_checkpoint(Arena* arena)
{
    return arena->size;
}

void arena_restore_to_checkpoint(Arena* arena, size_t checkpoint)
{
    arena->size = checkpoint;
}

void arena_free(Arena* arena)
{
    munmap(arena->base, DEFAULT_ARENA_SIZE);
}
