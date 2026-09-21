#pragma once

#include "../ceres.h"

// Text framebuffer (0xFF030000): a grid of characters a program draws into and then shows. Not pixels
// and not a scrolling stream: a board redrawn whole, which is what a text game wants.
// See CeresASM docs/07-IO-Devices-and-Ports.md.
//
// The device holds up to 200 x 100 cells of one printable ASCII byte each (anything else shows as a
// space) and an attribute byte for its colours; the default is 40 x 20.
//
// WHERE A FRAME GOES. A machine has a screen, so a frame is shown in the host's WINDOW: the grid drawn in a bitmap
// font, in its colours, in a window that opens when the first frame is shown (a program that never shows one opens
// none). Where the host has no window - a build without SDL, a run started with --terminal, or CERES_HEADLESS set in
// the environment, which scripts and tests do - a frame is printed to the host's standard output instead, one line
// per row (with the escape sequences of the colours, if any). fb_set_output() chooses at run time: FB_OUT_TERMINAL
// for the text on the terminal even where there is a window, FB_OUT_WINDOW or FB_OUT_AUTO (the default) for the
// window where there is one. fb_output() says where a frame goes now. A program that wants the text where
// ansi.h can draw in place around it uses the terminal.
//
// In the window the keys reach the program on the keyboard device (ceres/key.h reads them), and the program's
// own printf still goes to the terminal that started it.
//
// COLOUR. An attribute of 0 is the terminal's own colours. Any other is FB_ATTR(foreground, background),
// each one of the eight ANSI colours (FB_BLACK ... FB_WHITE) or its bright version (FB_BRIGHT + colour);
// black on black is the default, since it would not show. fb_set_attr() sets the attribute every later
// drawing call gives the cells it writes, fb_clear() included, and fb_put_attr() writes one cell with its
// own. A frame with any coloured cell is shown with escape sequences (each row ends back on the default
// colours); one with none is plain text.
//
// This module keeps its own copy of the grid in RAM (malloc'd), draws into that, and fb_present() sends
// it to the device in ONE block transfer and asks it to show the frame. Drawing is therefore free of
// device traffic, and a frame never appears half drawn. Every drawing call is clipped to the grid: a
// point or a run outside it is simply not drawn.

#define FB_CMD     (FRAMEBUFFER_BASE + 0x00)   // W: 1 clears and rewinds, 2 shows the frame
#define FB_WIDTH   (FRAMEBUFFER_BASE + 0x04)   // RW: columns, at most 200 (resizing clears the device)
#define FB_HEIGHT  (FRAMEBUFFER_BASE + 0x08)   // RW: rows, at most 100
#define FB_DATA    (FRAMEBUFFER_BASE + 0x0C)   // W: one cell, continuing where the last write stopped
#define FB_MODE    (FRAMEBUFFER_BASE + 0x10)   // RW: where frames should go: FB_OUT_AUTO, FB_OUT_TERMINAL or FB_OUT_WINDOW
#define FB_OUTPUT  (FRAMEBUFFER_BASE + 0x14)   // R: where they go now: FB_OUT_TERMINAL or FB_OUT_WINDOW
#define FB_BLOCK_ADDR (FRAMEBUFFER_BASE + 0xF0)
#define FB_BLOCK_LEN  (FRAMEBUFFER_BASE + 0xF4)
#define FB_BLOCK_CMD  (FRAMEBUFFER_BASE + 0xF8)   // W: 2 writes a run of cells from RAM, 3 a run of attributes

#define FB_CMD_CLEAR    1
#define FB_CMD_PRESENT  2

#define FB_OUT_AUTO      0                         // the window if the host has one, else the terminal (the default)
#define FB_OUT_TERMINAL  1                         // text on the terminal
#define FB_OUT_WINDOW    2                         // the window; the terminal where there is none

#define FB_BLACK    0
#define FB_RED      1
#define FB_GREEN    2
#define FB_YELLOW   3
#define FB_BLUE     4
#define FB_MAGENTA  5
#define FB_CYAN     6
#define FB_WHITE    7
#define FB_BRIGHT   8                              // added to a colour: FB_BRIGHT + FB_RED
#define FB_ATTR(fg, bg)  ((unsigned char)(((bg) << 4) | (fg)))
#define FB_MAX_COLS     200
#define FB_MAX_ROWS     100

void fb_set_output(int output);        // FB_OUT_AUTO, FB_OUT_TERMINAL or FB_OUT_WINDOW; anything else is ignored
int  fb_output(void);                      // where a frame goes now: FB_OUT_TERMINAL or FB_OUT_WINDOW

int  fb_init(int cols, int rows);          // sets the device size and allocates the grid; 0 ok, -1 if it does not fit
void fb_shutdown(void);                    // frees the grid
int  fb_cols(void);
int  fb_rows(void);

void fb_clear(char c);                     // fills the whole grid with c (use ' ' for blank)
void fb_present(void);                     // sends the grid to the device and shows it

void fb_set_attr(unsigned char attr);     // the attribute later drawing gives its cells; 0 = the terminal's colours
unsigned char fb_attr(void);               // ... as last set
void fb_put_attr(int x, int y, char c, unsigned char attr);   // one cell with its own attribute
unsigned char fb_get_attr(int x, int y);   // 0 outside the grid

void fb_put(int x, int y, char c);
char fb_get(int x, int y);                 // ' ' outside the grid

void fb_text(int x, int y, const char* s);            // a '\n' moves to the next row, back at column x
void fb_printf(int x, int y, const char* fmt, ...);   // formatted text, at most 255 characters
void fb_hline(int x, int y, int len, char c);
void fb_vline(int x, int y, int len, char c);
void fb_rect(int x, int y, int w, int h, char c);     // the outline
void fb_fill(int x, int y, int w, int h, char c);
void fb_box(int x, int y, int w, int h);              // a frame drawn with + - |
void fb_copy(int dx, int dy, int sx, int sy, int w, int h);   // a region onto another; overlap is handled
void fb_scroll(int dy);                    // moves the content up by dy rows (down when negative); new rows are blank
