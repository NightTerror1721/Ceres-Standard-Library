#pragma once

#include "gfx.h"

// An 8x8 bitmap font for the pixel surfaces: printable ASCII (32..126), drawn by whole-number scale.
// Each glyph is eight rows of a byte, top to bottom, bit 0 the leftmost pixel; the letter itself is 5
// pixels wide and 7 tall, so there is a blank column and row between characters. Characters outside
// 32..126 draw as blanks (127 is an empty box).
//
// Text is drawn in the foreground colour only - the background is left as it was - onto the surface you
// pass, or onto the current target when that is NULL.

extern const unsigned char font8x8[128][8];

#define FONT_W 8                       // the advance of one character, before scaling
#define FONT_H 8                       // the height of a line, before scaling

void font_char(struct gfx_surface* s, int x, int y, char c, unsigned int fg, int scale);
void font_text(struct gfx_surface* s, int x, int y, const char* str, unsigned int fg, int scale);   // '\n' starts a new line at x
void font_printf(struct gfx_surface* s, int x, int y, unsigned int fg, int scale, const char* fmt, ...);   // up to 255 characters
int  font_text_width(const char* str, int scale);          // pixels: the longest line
int  font_text_height(const char* str, int scale);         // pixels: every line
