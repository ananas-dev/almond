#pragma once

#include "arena.h"

#define Array(type)      \
    struct {             \
        type* items;     \
        size_t count;    \
        size_t capacity; \
    }

#define ArrayAdd(arena, array, value)                                                 \
    do {                                                                              \
        if ((array)->count >= (array)->capacity) {                                    \
            size_t new_capacity = (array)->capacity == 0 ? 8 : (array)->capacity * 2; \
            (array)->items = arena_realloc(                                           \
                (arena),                                                              \
                (array)->items,                                                       \
                (array)->capacity * sizeof(*(array)->items),                          \
                new_capacity * sizeof(*(array)->items),                               \
                16);                                                                  \
            (array)->capacity = new_capacity;                                         \
        }                                                                             \
        (array)->items[(array)->count++] = (value);                                   \
    } while (0)
