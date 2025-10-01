#pragma once

#include <cstddef>

struct TempMemory {
    void* start;
};

class Arena {
public:
    Arena(void* base, size_t size);
    void* alloc(size_t size, size_t alignment = 16);
    void* alloc_zero(size_t size, size_t alignment = 16);
    void* realloc(void* ptr, size_t old_size, size_t new_size, size_t alignment = 16);
    TempMemory begin_temp_memory();
    void end_temp_memory(TempMemory temp_memory);
    void clear();

    template <typename T>
    T* Push()
    {
        return static_cast<T*>(alloc(sizeof(T)));
    }

    template <typename T>
    T* PushArray(size_t count)
    {
        return static_cast<T*>(alloc(sizeof(T) * count));
    }

private:
    void* m_base;
    void* m_current;
    size_t m_size;
};

// #define PushArray(arena, type, count) (type *)arena_push((arena), sizeof(type)*(count), 16)
// #define PushArrayZero(arena, type, count) (type *)arena_push_zero((arena), sizeof(type)*(count), 16)
// #define PushStruct(arena, type) PushArray((arena), type, 1)
// #define PushStructZero(arena, type) PushArrayZero((arena), type, 1)
