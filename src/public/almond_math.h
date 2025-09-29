#pragma once
#include <math.h>

typedef union {
    struct {
        float x, y;
    };
    float items[2];
} Vector2;

#define VEC2(x, y)   \
    (Vector2)        \
    {                \
        {            \
            (x), (y) \
        }            \
    }

static inline Vector2 vector2_add(Vector2 a, Vector2 b)
{
    return VEC2(a.x + b.x, a.y + b.y);
}

static inline Vector2 vector2_sub(Vector2 a, Vector2 b)
{
    return VEC2(a.x - b.x, a.y - b.y);
}

static inline Vector2 vector2_normalize(Vector2 v)
{
    float length = sqrtf(v.x * v.x + v.y * v.y);

    if (length < 1e-6) {
        return VEC2(0, 0);
    }

    return VEC2(
        v.x / length,
        v.y / length);
}

typedef union {
    struct {
        float x, y, z;
    };
    float items[3];
} Vector3;

#define VEC3(x, y, z)     \
    (Vector3)             \
    {                     \
        {                 \
            (x), (y), (z) \
        }                 \
    }

#define VEC3_ZERO VEC3(0.0f, 0.0f, 0.0f)

static inline Vector3 vector3_add(Vector3 a, Vector3 b)
{
    return VEC3(a.x + b.x, a.y + b.y, a.z + b.z);
}

static inline Vector3 vector3_sub(Vector3 a, Vector3 b)
{
    return VEC3(a.x - b.x, a.y - b.y, a.z - b.z);
}

static inline Vector3 vector3_divs(Vector3 a, float b)
{
    return VEC3(a.x / b, a.y / b, a.z / b);
}

static inline float vector3_dot(Vector3 a, Vector3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline Vector3 vector3_cross(Vector3 a, Vector3 b)
{
    return VEC3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x);
}

static inline Vector3 vector3_normalize(Vector3 v)
{
    float length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);

    if (length < 1e-6) {
        return VEC3(0, 0, 0);
    }

    return VEC3(
        v.x / length,
        v.y / length,
        v.z / length);
}

static inline Vector3 vector3_scale(Vector3 v, float scale)
{
    return VEC3(
        v.x * scale,
        v.y * scale,
        v.z * scale);
}

static inline bool vector3_eq(Vector3 a, Vector3 b, float eps)
{
    float dist_2 = (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z);
    return dist_2 <= eps * eps;
}

static inline Vector3 vector3_lerp(Vector3 a, Vector3 b, float t)
{
    return vector3_add(vector3_scale(a, 1 - t), vector3_scale(b, t));
}

typedef union {
    struct {
        float x, y, z, w;
    };
    struct {
        float r, g, b, a;
    };
    float items[4];
} Vector4;
