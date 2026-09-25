# `<ceres/gfx.h>`

2D drawing on the pixel display. Everything is drawn into a surface in RAM and shown with gfx_present() in one block transfer, so a half-drawn frame is never seen. All of it is integer arithmetic (Bresenham lines, midpoint circles, scanline triangles).

```c
gfx_init(320, 200);
for (;;) { gfx_clear(COL_BLACK); gfx_circle_fill(160, 100, 20, COL_RED); gfx_present(); }
```

Drawing goes to the current TARGET: the screen surface until gfx_set_target() names another. Every primitive is clipped to the target's clip rectangle before it touches memory, so drawing partly (or wholly) outside is safe and simply loses the part that falls outside.

Memory: a surface takes width * height * 4 bytes - 250 KiB at 320x200, 900 KiB at 640x360, 3.5 MiB at 1280x720.

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
int  gfx_init(int w, int h);                 // sets the display size and allocates the screen surface; 0 ok, -1 when refused or out of memory
void gfx_shutdown(void);                     // frees the screen surface
struct gfx_surface* gfx_screen(void);        // NULL before gfx_init
int  gfx_width(void);                        // of the current target
int  gfx_height(void);
void gfx_present(void);                      // sends the screen surface to the display and presents it
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
