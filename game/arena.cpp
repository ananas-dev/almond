#include "arena.h"

#include <assert.h>
#include <cstdint>
#include <cstring>

#define ALIGN_TO(n, alignment) (((n) + (alignment) - 1) & ~((alignment) - 1))

Arena::Arena(void* base, size_t size)
    : m_base(base)
    , m_current(base)
    , m_size(size)
{
}

void* Arena::alloc(size_t size, size_t alignment)
{
    uintptr_t aligned = ALIGN_TO(reinterpret_cast<uintptr_t>(m_current), alignment);

    if (aligned + size > reinterpret_cast<uintptr_t>(m_base) + m_size) {
        return nullptr;
    }

    void* result = reinterpret_cast<void*>(aligned);
    m_current = reinterpret_cast<void*>(aligned + size);
    return result;
}

void* Arena::alloc_zero(size_t size, size_t alignment)
{
    void* ptr = alloc(size, alignment);
    if (ptr == nullptr)
        return nullptr;
    memset(ptr, 0, size);
    return ptr;
}

void* Arena::realloc(void* ptr, size_t old_size, size_t new_size, size_t alignment)
{
    if (new_size <= old_size) {
        return ptr;
    }

    auto uint_ptr = reinterpret_cast<uintptr_t>(ptr);
    auto uint_current = reinterpret_cast<uintptr_t>(m_current);

    if (uint_ptr + old_size == uint_current) {
        m_current = reinterpret_cast<void*>(uint_current + new_size - old_size);
        return ptr;
    }

    void* new_ptr = alloc(new_size, alignment);
    memcpy(new_ptr, ptr, old_size);

    return new_ptr;
}

void Arena::clear()
{
    m_current = m_base;
}

TempMemory Arena::begin_temp_memory()
{
    return { m_current };
}

void Arena::end_temp_memory(TempMemory temp_memory)
{
    m_current = temp_memory.start;
}


// void arena_pop(Arena* arena, size_t size)
// {
//     if (arena->current - size < arena->base) {
//         assert(false);
//         return;
//     }
//
//     arena->current -= size;
// }
//
// void* arena_realloc(Arena* arena, void* ptr, size_t old_size, size_t new_size, int alignment)
// {

// }
