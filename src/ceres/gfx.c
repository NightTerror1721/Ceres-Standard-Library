// 2D drawing. See ceres/gfx.h.
#include "ceres/gfx.h"
#include "ceres/display.h"
#include "ceres/blitter.h"
#include "stdlib.h"
#include "string.h"

static struct gfx_surface screen_surface;
static int have_screen = 0;
static struct gfx_surface* target;                       // NULL until the first use: then the screen
static int clip_x0 = 0, clip_y0 = 0, clip_x1 = 0, clip_y1 = 0;      // [x0, x1) x [y0, y1)
static int use_blitter = 1;                                           // and one is there (asked once)
static int blitter_known = 0, blitter_here = 0;

void gfx_use_blitter(int on)
{
    use_blitter = on;
}

static int blitter(void)
{
    if (!use_blitter)
        return 0;
    if (!blitter_known)
    {
        blitter_here = blitter_available();
        blitter_known = 1;
    }
    return blitter_here;
}

static struct gfx_surface* current(void)
{
    if (target == NULL && have_screen)
        target = &screen_surface;
    return target;
}

// ---- the screen ----

int gfx_init(int w, int h)
{
    if (w <= 0 || h <= 0 || w > DISP_MAX_WIDTH || h > DISP_MAX_HEIGHT)
        return -1;
    unsigned int* px = (unsigned int*)malloc((size_t)w * (size_t)h * 4u);
    if (px == NULL)
        return -1;
    if (display_size(w, h) != 0)
    {
        free(px);
        return -1;
    }
    if (have_screen)
        free(screen_surface.px);
    screen_surface.px = px;
    screen_surface.w = w;
    screen_surface.h = h;
    memset32(px, 0, (size_t)w * (size_t)h);
    have_screen = 1;
    gfx_set_target(NULL);
    return 0;
}

void gfx_shutdown(void)
{
    if (!have_screen)
        return;
    if (target == &screen_surface)
        target = NULL;
    free(screen_surface.px);
    screen_surface.px = NULL;
    screen_surface.w = screen_surface.h = 0;
    have_screen = 0;
}

struct gfx_surface* gfx_screen(void)
{
    return have_screen ? &screen_surface : NULL;
}

int gfx_width(void)
{
    struct gfx_surface* t = current();
    return t == NULL ? 0 : t->w;
}

int gfx_height(void)
{
    struct gfx_surface* t = current();
    return t == NULL ? 0 : t->h;
}

void gfx_present(void)
{
    if (!have_screen)
        return;
    display_blit(screen_surface.px, screen_surface.w, screen_surface.h);
    display_show();
}

// ---- targets and clipping ----

void gfx_reset_clip(void)
{
    struct gfx_surface* t = current();
    clip_x0 = 0;
    clip_y0 = 0;
    clip_x1 = t == NULL ? 0 : t->w;
    clip_y1 = t == NULL ? 0 : t->h;
}

void gfx_set_target(struct gfx_surface* s)
{
    target = s != NULL ? s : (have_screen ? &screen_surface : NULL);
    gfx_reset_clip();
}

struct gfx_surface* gfx_target(void)
{
    return current();
}

void gfx_set_clip(int x, int y, int w, int h)
{
    struct gfx_surface* t = current();
    if (t == NULL)
        return;
    int x1 = x + w, y1 = y + h;
    clip_x0 = x < 0 ? 0 : x;
    clip_y0 = y < 0 ? 0 : y;
    clip_x1 = x1 > t->w ? t->w : x1;
    clip_y1 = y1 > t->h ? t->h : y1;
    if (w <= 0 || clip_x1 < clip_x0) clip_x1 = clip_x0;
    if (h <= 0 || clip_y1 < clip_y0) clip_y1 = clip_y0;
}

void gfx_get_clip(int* x, int* y, int* w, int* h)
{
    if (x != NULL) *x = clip_x0;
    if (y != NULL) *y = clip_y0;
    if (w != NULL) *w = clip_x1 - clip_x0;
    if (h != NULL) *h = clip_y1 - clip_y0;
}

// ---- pixels and spans ----

void gfx_clear(unsigned int c)
{
    struct gfx_surface* t = current();
    if (t == NULL)
        return;
    if (blitter() && blitter_fill(t->px, t->w * 4, t->w, t->h, c) == 0)
        return;
    memset32(t->px, c, (size_t)t->w * (size_t)t->h);
}

void gfx_pixel(int x, int y, unsigned int c)
{
    struct gfx_surface* t = current();
    if (t != NULL && x >= clip_x0 && x < clip_x1 && y >= clip_y0 && y < clip_y1)
        t->px[y * t->w + x] = c;
}

unsigned int gfx_get(int x, int y)
{
    struct gfx_surface* t = current();
    if (t == NULL || x < 0 || x >= t->w || y < 0 || y >= t->h)
        return 0;
    return t->px[y * t->w + x];
}

void gfx_hline(int x, int y, int len, unsigned int c)
{
    struct gfx_surface* t = current();
    if (t == NULL || len <= 0 || y < clip_y0 || y >= clip_y1)
        return;
    int x0 = x, x1 = x + len;
    if (x0 < clip_x0) x0 = clip_x0;
    if (x1 > clip_x1) x1 = clip_x1;
    if (x1 > x0)
        memset32(t->px + y * t->w + x0, c, (size_t)(x1 - x0));
}

void gfx_vline(int x, int y, int len, unsigned int c)
{
    struct gfx_surface* t = current();
    if (t == NULL || len <= 0 || x < clip_x0 || x >= clip_x1)
        return;
    int y0 = y, y1 = y + len;
    if (y0 < clip_y0) y0 = clip_y0;
    if (y1 > clip_y1) y1 = clip_y1;
    for (int yy = y0; yy < y1; yy++)
        t->px[yy * t->w + x] = c;
}

void gfx_rect_fill(int x, int y, int w, int h, unsigned int c)
{
    if (w <= 0 || h <= 0)
        return;
    int y0 = y < clip_y0 ? clip_y0 : y;
    int y1 = y + h > clip_y1 ? clip_y1 : y + h;
    int x0 = x < clip_x0 ? clip_x0 : x;
    int x1 = x + w > clip_x1 ? clip_x1 : x + w;
    struct gfx_surface* t = current();
    if (t != NULL && x1 > x0 && y1 > y0 && blitter() &&
        blitter_fill(t->px + y0 * t->w + x0, t->w * 4, x1 - x0, y1 - y0, c) == 0)
        return;
    for (int yy = y0; yy < y1; yy++)
        gfx_hline(x, yy, w, c);
}

void gfx_rect(int x, int y, int w, int h, unsigned int c)
{
    if (w <= 0 || h <= 0)
        return;
    gfx_hline(x, y, w, c);
    if (h > 1)
        gfx_hline(x, y + h - 1, w, c);
    if (h > 2)
    {
        gfx_vline(x, y + 1, h - 2, c);
        if (w > 1)
            gfx_vline(x + w - 1, y + 1, h - 2, c);
    }
}

// ---- lines ----

void gfx_line(int x0, int y0, int x1, int y1, unsigned int c)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;               // minus the vertical distance
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;)
    {
        gfx_pixel(x0, y0, c);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

// ---- circles ----

static void circle_points(int cx, int cy, int x, int y, unsigned int c)
{
    gfx_pixel(cx + x, cy + y, c);
    gfx_pixel(cx - x, cy + y, c);
    gfx_pixel(cx + x, cy - y, c);
    gfx_pixel(cx - x, cy - y, c);
    gfx_pixel(cx + y, cy + x, c);
    gfx_pixel(cx - y, cy + x, c);
    gfx_pixel(cx + y, cy - x, c);
    gfx_pixel(cx - y, cy - x, c);
}

void gfx_circle(int cx, int cy, int r, unsigned int c)
{
    if (r < 0)
        return;
    int x = r, y = 0, err = 1 - r;
    while (x >= y)
    {
        circle_points(cx, cy, x, y, c);
        y++;
        if (err < 0)
            err += 2 * y + 1;
        else
        {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

void gfx_circle_fill(int cx, int cy, int r, unsigned int c)
{
    if (r < 0)
        return;
    int x = r, y = 0, err = 1 - r;
    while (x >= y)
    {
        gfx_hline(cx - x, cy + y, 2 * x + 1, c);
        gfx_hline(cx - x, cy - y, 2 * x + 1, c);
        gfx_hline(cx - y, cy + x, 2 * y + 1, c);
        gfx_hline(cx - y, cy - x, 2 * y + 1, c);
        y++;
        if (err < 0)
            err += 2 * y + 1;
        else
        {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

// ---- triangles ----

void gfx_triangle(int x0, int y0, int x1, int y1, int x2, int y2, unsigned int c)
{
    gfx_line(x0, y0, x1, y1, c);
    gfx_line(x1, y1, x2, y2, c);
    gfx_line(x2, y2, x0, y0, c);
}

// The x where the edge (xa,ya)-(xb,yb) crosses row y, rounded to nearest. ya != yb.
static int edge_x(int xa, int ya, int xb, int yb, int y)
{
    int num = (xb - xa) * (y - ya);
    int den = yb - ya;
    int half = den > 0 ? den / 2 : -(den / 2);
    if ((num < 0) != (den < 0))
        num -= half;
    else
        num += half;
    return xa + num / den;
}

void gfx_triangle_fill(int x0, int y0, int x1, int y1, int x2, int y2, unsigned int c)
{
    int t;
    // sort the corners by y: (x0,y0) is the top, (x2,y2) the bottom
    if (y0 > y1) { t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }
    if (y1 > y2) { t = x1; x1 = x2; x2 = t; t = y1; y1 = y2; y2 = t; }
    if (y0 > y1) { t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }

    if (y0 == y2)                                       // flat: one row, from the leftmost to the rightmost corner
    {
        int lo = x0, hi = x0;
        if (x1 < lo) lo = x1;
        if (x1 > hi) hi = x1;
        if (x2 < lo) lo = x2;
        if (x2 > hi) hi = x2;
        gfx_hline(lo, y0, hi - lo + 1, c);
        return;
    }

    for (int y = y0; y <= y2; y++)
    {
        int a = edge_x(x0, y0, x2, y2, y);              // the long edge, top to bottom
        int b;
        if (y < y1)
            b = edge_x(x0, y0, x1, y1, y);              // then the upper short edge...
        else if (y == y1)
            b = x1;                                     // ...the middle corner itself (also a flat top)...
        else
            b = edge_x(x1, y1, x2, y2, y);              // ...and the lower short edge
        int lo = a < b ? a : b;
        int hi = a < b ? b : a;
        gfx_hline(lo, y, hi - lo + 1, c);
    }
}

// ---- surfaces and copying ----

struct gfx_surface* gfx_surface_new(int w, int h)
{
    if (w <= 0 || h <= 0)
        return NULL;
    size_t bytes = (size_t)w * (size_t)h * 4u;
    struct gfx_surface* s = (struct gfx_surface*)malloc(sizeof(struct gfx_surface) + bytes);
    if (s == NULL)
        return NULL;
    s->px = (unsigned int*)(s + 1);
    s->w = w;
    s->h = h;
    memset32(s->px, 0, (size_t)w * (size_t)h);
    return s;
}

void gfx_surface_free(struct gfx_surface* s)
{
    if (s == NULL)
        return;
    if (s == target)
        gfx_set_target(NULL);
    free(s);
}

// Limits a copy of a w x h block from (sx, sy) of `src` to (dx, dy) so that it stays inside the source
// and inside the target's clip rectangle. Returns 0 when nothing is left.
static int clip_copy(const struct gfx_surface* src, int* sx, int* sy, int* w, int* h, int* dx, int* dy)
{
    if (*sx < 0) { *dx -= *sx; *w += *sx; *sx = 0; }
    if (*sy < 0) { *dy -= *sy; *h += *sy; *sy = 0; }
    if (*sx + *w > src->w) *w = src->w - *sx;
    if (*sy + *h > src->h) *h = src->h - *sy;
    if (*dx < clip_x0) { *sx += clip_x0 - *dx; *w -= clip_x0 - *dx; *dx = clip_x0; }
    if (*dy < clip_y0) { *sy += clip_y0 - *dy; *h -= clip_y0 - *dy; *dy = clip_y0; }
    if (*dx + *w > clip_x1) *w = clip_x1 - *dx;
    if (*dy + *h > clip_y1) *h = clip_y1 - *dy;
    return *w > 0 && *h > 0;
}

void gfx_blit(const struct gfx_surface* src, int sx, int sy, int w, int h, int dx, int dy)
{
    struct gfx_surface* t = current();
    if (t == NULL || src == NULL || !clip_copy(src, &sx, &sy, &w, &h, &dx, &dy))
        return;
    if (blitter() && blitter_copy(t->px + dy * t->w + dx, t->w * 4, src->px + sy * src->w + sx, src->w * 4, w, h) == 0)
        return;                                          // it minds the overlap the same way
    // Copying a surface onto itself with the block moving DOWN would overwrite rows before they are read,
    // so that case runs bottom to top (memmove already takes care of overlap within a row).
    int backwards = src == t && dy > sy;
    for (int i = 0; i < h; i++)
    {
        int row = backwards ? h - 1 - i : i;
        memmove(t->px + (dy + row) * t->w + dx, src->px + (sy + row) * src->w + sx, (size_t)w * 4u);
    }
}

void gfx_blit_key(const struct gfx_surface* src, int sx, int sy, int w, int h, int dx, int dy, unsigned int key)
{
    struct gfx_surface* t = current();
    if (t == NULL || src == NULL || !clip_copy(src, &sx, &sy, &w, &h, &dx, &dy))
        return;
    if (blitter() && blitter_copy_key(t->px + dy * t->w + dx, t->w * 4, src->px + sy * src->w + sx, src->w * 4, w, h, key) == 0)
        return;
    for (int row = 0; row < h; row++)
    {
        const unsigned int* from = src->px + (sy + row) * src->w + sx;
        unsigned int* to = t->px + (dy + row) * t->w + dx;
        for (int col = 0; col < w; col++)
            if (from[col] != key)
                to[col] = from[col];
    }
}

void gfx_blit_scaled(const struct gfx_surface* src, int dx, int dy, int scale)
{
    if (src == NULL || scale < 1)
        return;
    struct gfx_surface* t = current();
    // All of it inside the clip, at a scale the blitter has: one operation.
    if (t != NULL && scale <= 8 && dx >= clip_x0 && dy >= clip_y0 && dx + src->w * scale <= clip_x1 &&
        dy + src->h * scale <= clip_y1 && blitter() &&
        blitter_copy_scaled(t->px + dy * t->w + dx, t->w * 4, src->px, src->w * 4, src->w, src->h, scale) == 0)
        return;
    for (int y = 0; y < src->h; y++)
    {
        int top = dy + y * scale;
        if (top >= clip_y1 || top + scale <= clip_y0)
            continue;
        for (int x = 0; x < src->w; x++)
            gfx_rect_fill(dx + x * scale, top, scale, scale, src->px[y * src->w + x]);
    }
}
