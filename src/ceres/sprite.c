// Sprites, animation, tile maps, rectangles. See ceres/sprite.h.
#include "ceres/sprite.h"

// A sprite is a surface to the blitters: same layout, read only.
static struct gfx_surface view(const struct sprite* s)
{
    struct gfx_surface v;
    v.px = (unsigned int*)s->px;
    v.w = s->w;
    v.h = s->h;
    return v;
}

void sprite_draw(const struct sprite* s, int x, int y)
{
    if (s == NULL)
        return;
    struct gfx_surface v = view(s);
    gfx_blit_key(&v, 0, 0, s->w, s->h, x, y, s->key);
}

void sprite_draw_flip(const struct sprite* s, int x, int y, int flip_x, int flip_y)
{
    if (s == NULL)
        return;
    if (!flip_x && !flip_y)
    {
        sprite_draw(s, x, y);
        return;
    }
    for (int row = 0; row < s->h; row++)
    {
        int from_row = flip_y ? s->h - 1 - row : row;
        for (int col = 0; col < s->w; col++)
        {
            int from_col = flip_x ? s->w - 1 - col : col;
            unsigned int c = s->px[from_row * s->w + from_col];
            if (c != s->key)
                gfx_pixel(x + col, y + row, c);
        }
    }
}

void sprite_draw_scaled(const struct sprite* s, int x, int y, int scale)
{
    if (s == NULL || scale < 1)
        return;
    for (int row = 0; row < s->h; row++)
        for (int col = 0; col < s->w; col++)
        {
            unsigned int c = s->px[row * s->w + col];
            if (c != s->key)
                gfx_rect_fill(x + col * scale, y + row * scale, scale, scale, c);
        }
}

// ---- animation ----

void anim_init(struct anim* a, const struct sprite* const* frames, int count, int ticks_per_frame)
{
    a->frames = frames;
    a->count = count;
    a->ticks_per_frame = ticks_per_frame;
    a->index = 0;
    a->elapsed = 0;
}

void anim_reset(struct anim* a)
{
    a->index = 0;
    a->elapsed = 0;
}

void anim_update(struct anim* a, int ticks)
{
    if (a->count <= 0 || a->ticks_per_frame <= 0 || ticks <= 0)
        return;
    a->elapsed += ticks;
    while (a->elapsed >= a->ticks_per_frame)
    {
        a->elapsed -= a->ticks_per_frame;
        a->index = (a->index + 1) % a->count;
    }
}

const struct sprite* anim_frame(const struct anim* a)
{
    if (a->count <= 0 || a->frames == NULL)
        return NULL;
    int i = a->index;
    if (i < 0 || i >= a->count)
        i = 0;
    return a->frames[i];
}

// ---- tile maps ----

int tilemap_tile(const struct tilemap* m, int col, int row)
{
    if (col < 0 || row < 0 || col >= m->cols || row >= m->rows)
        return -1;
    return m->tiles[row * m->cols + col];
}

int tilemap_at(const struct tilemap* m, int px, int py)
{
    if (px < 0 || py < 0 || m->tile_w <= 0 || m->tile_h <= 0)
        return -1;
    return tilemap_tile(m, px / m->tile_w, py / m->tile_h);
}

void tilemap_draw(const struct tilemap* m, int cam_x, int cam_y)
{
    if (m->tile_w <= 0 || m->tile_h <= 0 || gfx_target() == NULL)
        return;
    int view_w = gfx_width(), view_h = gfx_height();

    // the range of cells that can be on screen
    int first_col = cam_x < 0 ? 0 : cam_x / m->tile_w;
    int first_row = cam_y < 0 ? 0 : cam_y / m->tile_h;
    int last_col = (cam_x + view_w - 1) / m->tile_w;
    int last_row = (cam_y + view_h - 1) / m->tile_h;
    if (cam_x + view_w <= 0 || cam_y + view_h <= 0)
        return;
    if (last_col >= m->cols) last_col = m->cols - 1;
    if (last_row >= m->rows) last_row = m->rows - 1;

    for (int row = first_row; row <= last_row; row++)
        for (int col = first_col; col <= last_col; col++)
        {
            const struct sprite* tile = m->tileset[m->tiles[row * m->cols + col]];
            if (tile != NULL)
                sprite_draw(tile, col * m->tile_w - cam_x, row * m->tile_h - cam_y);
        }
}

// ---- rectangles ----

int rect_overlap(const struct rect* a, const struct rect* b)
{
    return a->w > 0 && a->h > 0 && b->w > 0 && b->h > 0 &&
           a->x < b->x + b->w && b->x < a->x + a->w &&
           a->y < b->y + b->h && b->y < a->y + a->h;
}

int rect_contains(const struct rect* r, int x, int y)
{
    return x >= r->x && y >= r->y && x < r->x + r->w && y < r->y + r->h;
}
