# `<ceres/color.h>`

Colours are 0x00RRGGBB in an unsigned int, the format of the pixel display. Channels are 0..255.

```c
#ifndef RGB
#define RGB(r, g, b) (((unsigned int)(r) << 16) | ((unsigned int)(g) << 8) | (unsigned int)(b))
#endif

#define COL_R(c) (((c) >> 16) & 255)
#define COL_G(c) (((c) >> 8) & 255)
#define COL_B(c) ((c) & 255)

#define COL_BLACK   0x000000
#define COL_WHITE   0xFFFFFF
#define COL_RED     0xFF0000
#define COL_GREEN   0x00FF00
#define COL_BLUE    0x0000FF
#define COL_YELLOW  0xFFFF00
#define COL_CYAN    0x00FFFF
#define COL_MAGENTA 0xFF00FF
#define COL_GRAY    0x808080

unsigned int color_hsv(int h, int s, int v);                        // hue in degrees (any integer: 360 wraps), saturation and value 0..255
unsigned int color_lerp(unsigned int a, unsigned int b, int t);     // t 0..255: 0 gives a, 255 gives b (values outside are clamped)
unsigned int color_blend(unsigned int dst, unsigned int src, int alpha);   // src over dst; alpha 0 leaves dst, 255 is src
unsigned int color_scale(unsigned int c, int factor);               // every channel * factor/256, clamped: 256 leaves it, 128 halves, 512 doubles

// The 16 colours of the CGA/EGA text palette, in the classic order (black, blue, green, cyan, red,
// magenta, brown, light gray, dark gray, then the bright versions).
extern const unsigned int PALETTE16[16];
```
