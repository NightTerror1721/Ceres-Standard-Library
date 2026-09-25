// Bitmap font. See ceres/font.h.
#include "ceres/font.h"
#include "ceres/utf8.h"
#include "stdio.h"
#include "stdarg.h"

#include "font_data.inc"

// Each function draws through the target machinery: switch to `s` for the duration, then put back both
// the target and the clip it had.
struct saved { struct gfx_surface* target; int cx, cy, cw, ch; };

static void enter(struct saved* v, struct gfx_surface* s)
{
    v->target = gfx_target();
    gfx_get_clip(&v->cx, &v->cy, &v->cw, &v->ch);
    if (s != NULL)
        gfx_set_target(s);
}

static void leave(const struct saved* v, struct gfx_surface* s)
{
    if (s == NULL)
        return;
    gfx_set_target(v->target);
    gfx_set_clip(v->cx, v->cy, v->cw, v->ch);
}

// A glyph onto the current target: a code point up to U+00FF is its own, anything above is the box of 127.
static void glyph_at(int x, int y, unsigned int cp, unsigned int fg, int scale)
{
    const unsigned char* glyph = font8x8[cp <= 255u ? cp : 127u];
    for (int row = 0; row < 8; row++)
    {
        unsigned int bits = glyph[row];
        if (bits == 0)
            continue;
        for (int col = 0; col < 8; col++)
            if ((bits >> col) & 1u)
                gfx_rect_fill(x + col * scale, y + row * scale, scale, scale, fg);
    }
}

void font_char32(struct gfx_surface* s, int x, int y, unsigned int cp, unsigned int fg, int scale)
{
    if (scale < 1)
        return;
    struct saved before;
    enter(&before, s);
    glyph_at(x, y, cp, fg, scale);
    leave(&before, s);
}

void font_char(struct gfx_surface* s, int x, int y, char c, unsigned int fg, int scale)
{
    font_char32(s, x, y, (unsigned char)c, fg, scale);   // the byte as Latin-1
}

void font_text(struct gfx_surface* s, int x, int y, const char* str, unsigned int fg, int scale)
{
    if (scale < 1)
        return;
    struct saved before;
    enter(&before, s);

    int cx = x, cy = y;
    unsigned int cp;
    while ((cp = utf8_next(&str)) != 0)
    {
        if (cp == '\n')
        {
            cx = x;
            cy += FONT_H * scale;
            continue;
        }
        glyph_at(cx, cy, cp, fg, scale);
        cx += FONT_W * scale;
    }

    leave(&before, s);
}

void font_printf(struct gfx_surface* s, int x, int y, unsigned int fg, int scale, const char* fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    font_text(s, x, y, buf, fg, scale);
}

int font_text_width(const char* str, int scale)
{
    int widest = 0, line = 0;
    unsigned int cp;
    while ((cp = utf8_next(&str)) != 0)
    {
        if (cp == '\n')
            line = 0;
        else
            line++;
        if (line > widest)
            widest = line;
    }
    return widest * FONT_W * scale;
}

int font_text_height(const char* str, int scale)
{
    int lines = 1;
    for (; *str != '\0'; str++)
        if (*str == '\n')
            lines++;
    return lines * FONT_H * scale;
}
