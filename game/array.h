#pragma once

#include "arena.h"

#define Array(type)      \
    struct {             \
        type* items;     \
        size_t count;    \
        size_t capacity; \
    }

#define ArrayAdd(arena, array, value)                                                                                                           \
    do {                                                                                                                                        \
        if ((array)->count >= (array)->capacity) {                                                                                              \
            (array)->items = arena_realloc((arena), (array)->items, (array)->capacity, (array)->capacity == 0 ? 8 : (array)->capacity * 2, 16); \
            (array)->capacity = (array)->capacity == 0 ? 8 : (array)->capacity * 2;                                                             \
            (array)->items[(array)->count++] = (value);                                                                                         \
        } else {                                                                                                                                \
            (array)->items[(array)->count++] = (value);                                                                                         \
        }                                                                                                                                       \
    } while (0)
