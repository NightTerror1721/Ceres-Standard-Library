#pragma once

#include "gfx.h"

// An 8x8 bitmap font for the pixel surfaces: printable ASCII (32..126) and Latin-1 (0xA0..0xFF, the code points
// U+00A0..U+00FF: accented letters, the Spanish and French punctuation, currency signs...), drawn by whole-number
// scale. Each glyph is eight rows of a byte, top to bottom, bit 0 the leftmost pixel; the letter itself is 5
// pixels wide and 7 tall, so there is a blank column and row between characters. The controls draw as blanks,
// and 127 is an empty box.
//
// font_text() and the rest take UTF-8 (ceres/utf8.h): "¿Qué año?" draws as nine characters, and a character above
// U+00FF, or a byte that is not UTF-8, as the box. font_char() draws one byte as Latin-1, font_char32() a code point.
//
// Text is drawn in the foreground colour only - the background is left as it was - onto the surface you
// pass, or onto the current target when that is NULL.

extern const unsigned char font8x8[256][8];

#define FONT_W 8                       // the advance of one character, before scaling
#define FONT_H 8                       // the height of a line, before scaling

void font_char(struct gfx_surface* s, int x, int y, char c, unsigned int fg, int scale);
void font_char32(struct gfx_surface* s, int x, int y, unsigned int cp, unsigned int fg, int scale);
void font_text(struct gfx_surface* s, int x, int y, const char* str, unsigned int fg, int scale);   // '\n' starts a new line at x
void font_printf(struct gfx_surface* s, int x, int y, unsigned int fg, int scale, const char* fmt, ...)
    __attribute__((__format__(__printf__, 6, 7)));   // up to 255 characters
int  font_text_width(const char* str, int scale);          // pixels: the longest line (in characters, not bytes)
int  font_text_height(const char* str, int scale);         // pixels: every line
