# `<ceres/sprite.h>`

Sprites (images with one transparent colour), frame animation, tile maps with a camera, and the rectangle tests that games need for collisions. Pixel data is the caller's: a sprite only points at it, so a sprite can live in .rodata.

```c
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
