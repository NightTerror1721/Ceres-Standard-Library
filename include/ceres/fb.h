#pragma once

#include "../stddef.h"
#include "video.h"
#include "color.h"

// The GPU's bitmap plane (level V1, CeresASM plan/v2 SPEC 7): a picture in video memory, behind the text plane,
// that the screen shows at every vertical blank.
//
//   fb_init(320, 240);                          // the screen at 320x240, a picture of its size
//   for (;;) {
//       unsigned int* px = fb_pixels();         // the buffer to draw into
//       ...px[y * 320 + x] = RGB(255, 0, 0)...
//       fb_present();                           // shown at the next blank; px is then the other buffer
//   }
//
// There are two buffers: the one shown and the one drawn into, and fb_present() swaps them at the vertical blank
// and waits for it, so a half-drawn frame is never seen and the program runs at the screen's pace (60 frames a
// second, in the machine's time). After the swap the buffer to draw into holds the frame before last: redraw it
// all, or fb_keep() to copy the one just shown into it.
//
// A pixel is 0x00RRGGBB (color.h: RGB), or with fb_init_indexed() a byte, an index into a palette of 256 colours:
// a quarter of the memory, and a colour changed in the palette changes everywhere at once. The text plane stays in
// front, see-through where its cells have a black background, so what the program prints appears over the picture.
// fb_scroll() moves the picture under the screen, wrapping round, so a background can move without being drawn again.

#define FB_ENABLE        (GPU_BASE + 0x240)   // RW: 1 shows the plane
#define FB_BASE          (GPU_BASE + 0x244)   // RW: the buffer shown
#define FB_BACK_BASE     (GPU_BASE + 0x248)   // RW: the buffer to draw into
#define FB_PITCH         (GPU_BASE + 0x24C)   // RW: bytes from a row to the next
#define FB_FORMAT        (GPU_BASE + 0x250)   // RW: FB_FORMAT_*
#define FB_PLANE_WIDTH   (GPU_BASE + 0x254)   // RW: the picture's size
#define FB_PLANE_HEIGHT  (GPU_BASE + 0x258)
#define FB_SCROLL_X      (GPU_BASE + 0x25C)   // RW: the picture's column at the screen's left edge
#define FB_SCROLL_Y      (GPU_BASE + 0x260)
#define FB_BUFFERS       (GPU_BASE + 0x264)   // RW: 1-3
#define FB_PALETTE_BASE  (GPU_BASE + 0x268)   // RW: 256 colours for the indexed formats
#define FB_SPARE_BASE    (GPU_BASE + 0x26C)   // RW: the third buffer

#define FB_FORMAT_I1        0
#define FB_FORMAT_I2        1
#define FB_FORMAT_I4        2
#define FB_FORMAT_I8        3
#define FB_FORMAT_RGB565    4
#define FB_FORMAT_ARGB1555  5
#define FB_FORMAT_XRGB8888  6
#define FB_FORMAT_ARGB8888  7

int  fb_init(int width, int height);            // the screen and a picture of width x height, 0x00RRGGBB; 0 ok, -1 when refused
int  fb_init_indexed(int width, int height);    // the same, a byte a pixel through the palette
void fb_shutdown(void);                         // the plane off, the screen back to text only
int  fb_width(void);                            // 0 before fb_init
int  fb_height(void);
int  fb_indexed(void);                          // 1 for fb_init_indexed

void* fb_pixels(void);                          // the buffer to draw into: unsigned int, or unsigned char when indexed
void fb_present(void);                          // swaps the buffers at the next vertical blank and waits for it
void fb_keep(void);                             // the frame just shown, copied into the buffer to draw into
void fb_clear(unsigned int value);              // the buffer to draw into, all of it: a colour, or an index
void fb_blit(const void* pixels);               // a whole picture from RAM into it (the copy engine)

void fb_set_palette(int index, unsigned int rgb);
void fb_load_palette(const unsigned int* colors, int count);   // entries 0..count-1
void fb_scroll(int x, int y);                   // taken modulo the size; negatives count from the other side
