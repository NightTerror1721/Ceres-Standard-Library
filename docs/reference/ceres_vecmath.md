# `<ceres/vecmath.h>`

2D and 3D vectors passed and returned by value, and the small numeric helpers games keep rewriting. It is vecmath.h, not vec.h, so it cannot be mistaken for ceres/ds/vector.h (the growable array).

```c
struct vec2 { float x; float y; };
struct vec3 { float x; float y; float z; };

static inline struct vec2 vec2_make(float x, float y) { struct vec2 v; v.x = x; v.y = y; return v; }
static inline struct vec2 vec2_add(struct vec2 a, struct vec2 b) { return vec2_make(a.x + b.x, a.y + b.y); }
static inline struct vec2 vec2_sub(struct vec2 a, struct vec2 b) { return vec2_make(a.x - b.x, a.y - b.y); }
static inline struct vec2 vec2_scale(struct vec2 a, float k)     { return vec2_make(a.x * k, a.y * k); }
static inline float vec2_dot(struct vec2 a, struct vec2 b)       { return a.x * b.x + a.y * b.y; }
static inline float vec2_len2(struct vec2 a)                     { return a.x * a.x + a.y * a.y; }
static inline float vec2_len(struct vec2 a)                      { return sqrt(vec2_len2(a)); }
struct vec2 vec2_normalize(struct vec2 a);                       // the zero vector stays zero
struct vec2 vec2_rotate(struct vec2 a, float radians);           // counter-clockwise for +y up

static inline struct vec3 vec3_make(float x, float y, float z) { struct vec3 v; v.x = x; v.y = y; v.z = z; return v; }
static inline struct vec3 vec3_add(struct vec3 a, struct vec3 b) { return vec3_make(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline struct vec3 vec3_sub(struct vec3 a, struct vec3 b) { return vec3_make(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline struct vec3 vec3_scale(struct vec3 a, float k)     { return vec3_make(a.x * k, a.y * k, a.z * k); }
static inline float vec3_dot(struct vec3 a, struct vec3 b)       { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline float vec3_len(struct vec3 a)                      { return sqrt(vec3_dot(a, a)); }
struct vec3 vec3_cross(struct vec3 a, struct vec3 b);
struct vec3 vec3_normalize(struct vec3 a);

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline int   clampi(int v, int lo, int hi)       { return v < lo ? lo : (v > hi ? hi : v); }
static inline float lerpf(float a, float b, float t)    { return a + (b - a) * t; }
static inline float signf(float v)                      { return v < 0.0f ? -1.0f : (v > 0.0f ? 1.0f : 0.0f); }
static inline float deg2rad(float d)                    { return d * (M_PI / 180.0f); }
static inline float rad2deg(float r)                    { return r * (180.0f / M_PI); }
float smoothstep(float e0, float e1, float x);                   // 0 below e0, 1 above e1, a cubic ease between
float wrapf(float v, float lo, float hi);                        // v brought into [lo, hi) by whole periods
```
