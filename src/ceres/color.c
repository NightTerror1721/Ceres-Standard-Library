// Colours. See ceres/color.h. Integer arithmetic only.
#include "ceres/color.h"

const unsigned int PALETTE16[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
};

// x / 255, rounded to nearest, for x in 0..65025 - the widest a*255 + b*255 gets. An arithmetic identity
// (no division): floor(t / 255) is (t + (t >> 8) + 1) >> 8 for t below 65536, and t = x + 127 stays below it.
static int div255r(int x)
{
    unsigned int t = (unsigned int)x + 127u;
    return (int)((t + (t >> 8) + 1u) >> 8);
}

// a * b / 255, rounded to nearest, for a and b in 0..255
static int mul255(int a, int b)
{
    return div255r(a * b);
}

static int clamp255(int v)
{
    return v < 0 ? 0 : (v > 255 ? 255 : v);
}

unsigned int color_hsv(int h, int s, int v)
{
    s = clamp255(s);
    v = clamp255(v);
    h %= 360;
    if (h < 0)
        h += 360;

    int sector = h / 60;
    int frac = ((h % 60) * 255 + 30) / 60;              // 0..255 across the sector, rounded to nearest
    int p = mul255(v, 255 - s);
    int q = mul255(v, 255 - mul255(s, frac));
    int t = mul255(v, 255 - mul255(s, 255 - frac));

    switch (sector)
    {
    case 0: return RGB(v, t, p);
    case 1: return RGB(q, v, p);
    case 2: return RGB(p, v, t);
    case 3: return RGB(p, q, v);
    case 4: return RGB(t, p, v);
    }
    return RGB(v, p, q);
}

unsigned int color_lerp(unsigned int a, unsigned int b, int t)
{
    t = clamp255(t);
    int r = div255r(COL_R(a) * (255 - t) + COL_R(b) * t);
    int g = div255r(COL_G(a) * (255 - t) + COL_G(b) * t);
    int bl = div255r(COL_B(a) * (255 - t) + COL_B(b) * t);
    return RGB(r, g, bl);
}

unsigned int color_blend(unsigned int dst, unsigned int src, int alpha)
{
    return color_lerp(dst, src, alpha);
}

unsigned int color_scale(unsigned int c, int factor)
{
    if (factor < 0)
        factor = 0;
    int r = (COL_R(c) * factor) >> 8;                   // / 256, the documented 8.8 factor
    int g = (COL_G(c) * factor) >> 8;
    int b = (COL_B(c) * factor) >> 8;
    return RGB(clamp255(r), clamp255(g), clamp255(b));
}
