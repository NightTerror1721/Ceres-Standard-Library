// The vector functions too long to live in the header. See ceres/vecmath.h.
#include "ceres/vecmath.h"

struct vec2 vec2_normalize(struct vec2 a)
{
    float len2 = vec2_len2(a);
    if (len2 == 0.0f)
        return a;
    return vec2_scale(a, rsqrt(len2));
}

struct vec2 vec2_rotate(struct vec2 a, float radians)
{
    float c = cos(radians);
    float s = sin(radians);
    return vec2_make(a.x * c - a.y * s, a.x * s + a.y * c);
}

struct vec3 vec3_cross(struct vec3 a, struct vec3 b)
{
    return vec3_make(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

struct vec3 vec3_normalize(struct vec3 a)
{
    float len2 = vec3_dot(a, a);
    if (len2 == 0.0f)
        return a;
    return vec3_scale(a, rsqrt(len2));
}

float smoothstep(float e0, float e1, float x)
{
    if (e0 == e1)
        return x < e0 ? 0.0f : 1.0f;
    float t = clampf((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float wrapf(float v, float lo, float hi)
{
    float period = hi - lo;
    if (period <= 0.0f)
        return lo;
    float r = v - lo;
    r -= floor(r / period) * period;
    if (r >= period)                    // rounding can land exactly on the far edge
        r = 0.0f;
    return lo + r;
}
