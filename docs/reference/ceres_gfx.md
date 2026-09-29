# `<ceres/gfx.h>`

2D drawing on the GPU's bitmap plane (ceres/fb.h). The screen surface is the plane's buffer to draw into, in video memory, and gfx_present() shows it at the next vertical blank - and waits for it, so a game drawn this way runs at the screen's 60 frames a second - then goes on drawing from the frame shown. A half-drawn frame is never seen. All of it is integer arithmetic (Bresenham lines, midpoint circles, scanline triangles).

```c
gfx_init(320, 200);
for (;;) { gfx_clear(COL_BLACK); gfx_circle_fill(160, 100, 20, COL_RED); gfx_present(); }
```

Drawing goes to the current TARGET: the screen surface until gfx_set_target() names another. Every primitive is clipped to the target's clip rectangle before it touches memory, so drawing partly (or wholly) outside is safe and simply loses the part that falls outside.

Memory: a surface of gfx_surface_new() takes width * height * 4 bytes of RAM - 250 KiB at 320x200, 900 KiB at 640x360, 3.5 MiB at 1280x720. The screen's two buffers take as much again of VRAM each.

On a machine with the blitter (ceres/blitter.h) clearing, filling rectangles and the three blits are done by it, at no cost in instructions; the result is the same pixel for pixel as the software path, which the rest takes.

```c
struct gfx_surface
{
    unsigned int* px;          // width * height pixels, row after row, no padding
    int w;
    int h;
};
```

## The screen

```c
int  gfx_init(int w, int h);                 // the screen at w x h with its bitmap plane (fb_init); 0 ok, -1 when refused or out of VRAM
void gfx_shutdown(void);                     // the bitmap plane off (fb_shutdown)
struct gfx_surface* gfx_screen(void);        // NULL before gfx_init
int  gfx_width(void);                        // of the current target
int  gfx_height(void);
void gfx_present(void);                      // shows the screen surface at the next vertical blank, and waits for it
void gfx_use_blitter(int on);                // 1 (the default): the blitter when there is one; 0: always in software
```

## Targets and clipping

```c
void gfx_set_target(struct gfx_surface* s);  // NULL goes back to the screen; resets the clip to the whole target
struct gfx_surface* gfx_target(void);
void gfx_set_clip(int x, int y, int w, int h);   // drawing is limited to this rectangle (itself limited to the target)
void gfx_reset_clip(void);
void gfx_get_clip(int* x, int* y, int* w, int* h);   // the clip now in force (any pointer may be NULL)
```

## Drawing on the target

```c
void gfx_clear(unsigned int c);              // the WHOLE target, whatever the clip
void gfx_pixel(int x, int y, unsigned int c);
unsigned int gfx_get(int x, int y);          // 0 outside the target
void gfx_hline(int x, int y, int len, unsigned int c);   // len pixels from x rightward
void gfx_vline(int x, int y, int len, unsigned int c);   // len pixels from y downward
void gfx_line(int x0, int y0, int x1, int y1, unsigned int c);     // both ends included
void gfx_rect(int x, int y, int w, int h, unsigned int c);         // outline
void gfx_rect_fill(int x, int y, int w, int h, unsigned int c);
void gfx_circle(int cx, int cy, int r, unsigned int c);            // outline; r 0 is one pixel
void gfx_circle_fill(int cx, int cy, int r, unsigned int c);
void gfx_triangle(int x0, int y0, int x1, int y1, int x2, int y2, unsigned int c);   // outline
void gfx_triangle_fill(int x0, int y0, int x1, int y1, int x2, int y2, unsigned int c);
```

## Surfaces and copying

```c
struct gfx_surface* gfx_surface_new(int w, int h);       // zero-filled (black); NULL when out of memory or the size is not positive
void gfx_surface_free(struct gfx_surface* s);            // safe on NULL; if it was the target, the screen becomes the target
// Copy the w x h block at (sx, sy) of src to (dx, dy) of the target.
void gfx_blit(const struct gfx_surface* src, int sx, int sy, int w, int h, int dx, int dy);
void gfx_blit_key(const struct gfx_surface* src, int sx, int sy, int w, int h, int dx, int dy, unsigned int key);   // pixels equal to key are skipped
void gfx_blit_scaled(const struct gfx_surface* src, int dx, int dy, int scale);   // the whole of src, each pixel scale x scale
```

## Software sprites, animation, tile maps, rectangles

```c
// Sprites (images with one transparent colour) drawn on the target, frame animation, tile maps with a camera, and
// the rectangle tests that games need for collisions. Pixel data is the caller's: a sprite only points at it, so a
// sprite can live in .rodata. (The GPU's own sprites and tile layers, level V2, are ceres/sprite.h and ceres/tiles.h.)

struct sprite
{
    int w;
    int h;
    const unsigned int* px;    // w * h pixels, row after row
    unsigned int key;          // pixels of this colour are transparent
};

// Frames shown in turn. `frames` is an array of `count` sprite pointers.
struct anim
{
    const struct sprite* const* frames;
    int count;
    int ticks_per_frame;       // how long each frame stays; <= 0 freezes the animation
    int index;                 // the frame now showing
    int elapsed;               // ticks spent on it
};

// A grid of tiles. `tiles` holds cols * rows tile numbers (row after row); `tileset` maps a number to the
// sprite to draw (a NULL entry draws nothing). All tiles are tile_w x tile_h.
struct tilemap
{
    int tile_w;
    int tile_h;
    int cols;
    int rows;
    const unsigned char* tiles;
    const struct sprite* const* tileset;
};

struct rect { int x; int y; int w; int h; };

void sprite_draw(const struct sprite* s, int x, int y);
void sprite_draw_flip(const struct sprite* s, int x, int y, int flip_x, int flip_y);   // mirrored horizontally and/or vertically
void sprite_draw_scaled(const struct sprite* s, int x, int y, int scale);              // each pixel scale x scale; the key still skips

void anim_init(struct anim* a, const struct sprite* const* frames, int count, int ticks_per_frame);
void anim_update(struct anim* a, int ticks);                 // advances by `ticks`, wrapping around at the last frame
const struct sprite* anim_frame(const struct anim* a);       // the frame to draw now (NULL for an empty animation)
void anim_reset(struct anim* a);

void tilemap_draw(const struct tilemap* m, int cam_x, int cam_y);   // (cam_x, cam_y) is the map pixel at the target's top-left corner; only visible tiles are drawn
int  tilemap_at(const struct tilemap* m, int px, int py);           // the tile number at map pixel (px, py), or -1 outside the map
int  tilemap_tile(const struct tilemap* m, int col, int row);       // the tile number at a grid cell, or -1 outside

int  rect_overlap(const struct rect* a, const struct rect* b);      // 1 when they share at least one pixel (touching edges do not count)
int  rect_contains(const struct rect* r, int x, int y);
```
