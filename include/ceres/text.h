#pragma once

#include "video.h"

// The GPU's text plane (level V0, CeresASM plan/v2 SPEC 7): a grid of 8x16 cells in video memory, drawn in front
// of everything else. Not a scrolling stream but a board redrawn whole, which is what a text game or a text
// interface wants; the terminal (ceres/terminal.h) draws in the same plane when the program prints.
//
// This module keeps its own copy of a grid in RAM, draws into that, and text_present() puts it on the screen at
// the top left in one go and waits for the vertical blank, so a frame never appears half drawn and every one is
// seen (`ceres run --screen-log` records each). The grid is at most the screen's: 80 x 30 cells at 640x480.
// While the module is in use the terminal's cursor is hidden.
//
// A cell holds one character - printable ASCII and Latin-1 (U+00A0..U+00FF, the accented letters, ¿ ¡ ° £...), and
// the box and block characters ─ │ ┌ ┐ └ ┘ ├ ┤ ┬ ┴ ┼ ═ ║ ╔ ╗ ╚ ╝ ╠ ╣ ╦ ╩ ╬ █ ▀ ▄ ░ ▒ ▓ ■ • ▲ ▼ - and an attribute
// for its colours. The text functions take UTF-8 (ceres/utf8.h), so text_text(0, 0, "¿Qué año?") fills nine
// cells; a character the font has not got is '?'. text_put() stores one byte as it is: its Latin-1 character
// (or, from 0x80 to 0x9F, a box character); text_put_char() a code point.
//
// COLOUR. An attribute is TEXT_ATTR(ink, background), each one of the eight colours (TEXT_BLACK ... TEXT_WHITE)
// or its bright version (TEXT_BRIGHT + colour). A background of TEXT_BLACK lets what is behind the text plane show
// through (the background colour, the bitmap). 0 is TEXT_NORMAL, the terminal's own colours: light grey on what is
// behind. text_set_attr() sets the attribute
// every later drawing call gives the cells it writes, text_clear() included, and text_put_attr() writes one cell
// with its own. Every drawing call is clipped to the grid: a point or a run outside it is not drawn.

#define TEXT_COLS          (GPU_BASE + 0x204)   // R: the screen's columns of cells
#define TEXT_ROWS          (GPU_BASE + 0x208)   // R: its rows
#define TEXT_CELLS_BASE    (GPU_BASE + 0x20C)   // RW: the first cell, in VRAM
#define TEXT_CELL_FORMAT   (GPU_BASE + 0x210)   // RW: 0 16-bit cells, 1 32-bit
#define TEXT_FONT_BASE     (GPU_BASE + 0x214)   // RW: 256 glyphs of 16 bytes
#define TEXT_PALETTE_BASE  (GPU_BASE + 0x21C)   // RW: 256 colours, 0x00RRGGBB
#define TEXT_ENABLE        (GPU_BASE + 0x200)   // RW: 1 shows the plane

#define TEXT_BLACK    0
#define TEXT_RED      1
#define TEXT_GREEN    2
#define TEXT_YELLOW   3
#define TEXT_BLUE     4
#define TEXT_MAGENTA  5
#define TEXT_CYAN     6
#define TEXT_WHITE    7
#define TEXT_BRIGHT   8                              // added to a colour: TEXT_BRIGHT + TEXT_RED
#define TEXT_ATTR(ink, bg)  ((unsigned char)(((bg) << 4) | (ink)))
#define TEXT_NORMAL   0                              // the terminal's own colours

int  text_init(int cols, int rows);         // allocates a grid, blank; 0 ok, -1 if it is larger than the screen or memory
void text_shutdown(void);                   // frees it, and the terminal's cursor shows again
int  text_cols(void);                       // of the grid
int  text_rows(void);
int  text_screen_cols(void);                // of the screen: the largest grid there can be
int  text_screen_rows(void);

void text_clear(char c);                    // fills the whole grid with c (use ' ' for blank)
void text_present(void);                    // puts the grid on the screen and waits for the vertical blank

void text_set_attr(unsigned char attr);     // the attribute later drawing gives its cells; 0 = TEXT_NORMAL
unsigned char text_attr(void);              // ... as last set
void text_put_attr(int x, int y, char c, unsigned char attr);   // one cell with its own attribute
unsigned char text_get_attr(int x, int y);  // 0 outside the grid

void text_put(int x, int y, char c);
char text_get(int x, int y);                // ' ' outside the grid

void text_text(int x, int y, const char* s);            // UTF-8; a '\n' moves to the next row, back at column x
int  text_text_n(int x, int y, const char* s, int max);  // at most max characters of one row (to a '\n'): those drawn
int  text_text_width(const char* s);                     // the cells of its longest line: characters, not bytes
void text_put_char(int x, int y, unsigned int cp);       // a code point, as its glyph, or '?'
unsigned char text_glyph(unsigned int cp);               // the glyph (the cell's byte) for a code point, or '?'
void text_printf(int x, int y, const char* fmt, ...) __attribute__((__format__(__printf__, 3, 4)));   // at most 255 characters
void text_hline(int x, int y, int len, char c);
void text_vline(int x, int y, int len, char c);
void text_rect(int x, int y, int w, int h, char c);     // the outline
void text_fill(int x, int y, int w, int h, char c);
void text_box(int x, int y, int w, int h);              // a frame drawn with the box characters ┌ ─ ┐ │ └ ┘
void text_copy(int dx, int dy, int sx, int sy, int w, int h);   // a region onto another; overlap is handled
void text_scroll(int dy);                   // moves the content up by dy rows (down when negative); new rows are blank
