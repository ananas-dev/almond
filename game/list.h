#pragma once
#include <cassert>
#include <cstddef>

template <typename T, typename Allocator>
class List {
public:
    explicit List(Allocator* allocator)
        : m_allocator(allocator)
    {
    }

    void reserve(size_t size)
    {
        size_t size_pow2;
        if (size == 0) {
            size_pow2 = 1;
        } else {
            size_pow2 = size - 1;
            size_pow2 |= size_pow2 >> 1;
            size_pow2 |= size_pow2 >> 2;
            size_pow2 |= size_pow2 >> 4;
            size_pow2 |= size_pow2 >> 8;
            size_pow2 |= size_pow2 >> 16;
            size_pow2 |= size_pow2 >> 32;
            size_pow2++;
        }

        if (size_pow2 <= capacity) {
            return;
        }

        items = m_allocator->realloc(
            items,
            capacity * sizeof(T),
            size_pow2 * sizeof(T));

        capacity = size_pow2;
    }

    void push(T item)
    {
        if (count >= capacity) {
            size_t new_capacity = capacity == 0 ? 8 : capacity * 2;
            items = static_cast<T*>(m_allocator->realloc(
                items,
                capacity * sizeof(T),
                new_capacity * sizeof(T)));
            capacity = new_capacity;
        };
        items[count++] = item;
    }

    T& operator[](size_t index)
    {
        assert(index < count && "Index out of bounds");
        return items[index];
    }

    const T& operator[](size_t index) const
    {
        assert(index < count && "Index out of bounds");
        return items[index];
    }

    T* items = nullptr;
    size_t count = 0;
    size_t capacity = 0;

private:
    Allocator* m_allocator;
};
