#pragma once

#include "../ceres.h"

// The 2D blitter (0xFF0C0000, CeresASM 3b2db3b): rectangle operations on RGB32 surfaces in RAM done by the host,
// at no cost in instructions. A surface is the address of a rectangle's first pixel and its stride (BYTES from
// one row to the next). ceres/gfx.h uses it on its own when it is there; these are for a program that draws
// into memory of its own. Every call returns 0, or -1 when there is no blitter or a row ran outside RAM.

#define BLIT_CMD       (BLITTER_BASE + 0x00)
#define BLIT_DST       (BLITTER_BASE + 0x04)
#define BLIT_DST_STRIDE (BLITTER_BASE + 0x08)
#define BLIT_SRC       (BLITTER_BASE + 0x0C)
#define BLIT_SRC_STRIDE (BLITTER_BASE + 0x10)
#define BLIT_WIDTH     (BLITTER_BASE + 0x14)
#define BLIT_HEIGHT    (BLITTER_BASE + 0x18)
#define BLIT_COLOR     (BLITTER_BASE + 0x1C)
#define BLIT_SCALE     (BLITTER_BASE + 0x20)
#define BLIT_PALETTE   (BLITTER_BASE + 0x24)
#define BLIT_CONTROL   (BLITTER_BASE + 0x28)   // bit 0: interrupt 25 when an operation is done
#define BLIT_STATUS    (BLITTER_BASE + 0x2C)   // bit 0: the last one failed
#define BLIT_PIXELS    (BLITTER_BASE + 0x30)   // pixels the last one wrote

#define BLIT_FILL              1
#define BLIT_COPY              2
#define BLIT_COPY_KEYED        3
#define BLIT_COPY_SCALED       4
#define BLIT_COPY_INDEXED      5
#define BLIT_COPY_INDEXED_KEYED 6

int blitter_available(void);
int blitter_fill(unsigned int* dst, int dst_stride, int w, int h, unsigned int color);
int blitter_copy(unsigned int* dst, int dst_stride, const unsigned int* src, int src_stride, int w, int h);   // overlap is fine
int blitter_copy_key(unsigned int* dst, int dst_stride, const unsigned int* src, int src_stride, int w, int h, unsigned int key);
int blitter_copy_scaled(unsigned int* dst, int dst_stride, const unsigned int* src, int src_stride, int w, int h, int scale);   // 1..8
// 8-bit pixels through a palette of 256 RGB32 colours; key -1 draws them all, 0..255 leaves that index out.
int blitter_copy_indexed(unsigned int* dst, int dst_stride, const unsigned char* src, int src_stride, int w, int h,
                         const unsigned int* palette, int key);
unsigned int blitter_last_pixels(void);   // how many pixels the last operation wrote
