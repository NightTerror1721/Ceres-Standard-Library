// Display device (0xFF070000): a pixel framebuffer of RGB32 pixels
// (0x00RRGGBB, top byte ignored). Distinct from the text framebuffer.
// A presented frame reaches the host through `ceres run --window` (SDL3) or a
// registered frame sink; without one, presenting is a no-op.
// See CeresASM docs/07-IO-Devices-and-Ports.md.

#pragma once

#include "../ceres.h"

#define DISP_CMD        (DISPLAY_BASE + 0x00)  // write: 1 clear, 2 present
#define DISP_WIDTH      (DISPLAY_BASE + 0x04)  // rw: pixel columns, <= 1280
#define DISP_HEIGHT     (DISPLAY_BASE + 0x08)  // rw: pixel rows, <= 720
#define DISP_DATA       (DISPLAY_BASE + 0x0C)  // write: one pixel per word (sequential)
#define DISP_BLOCK_ADDR (DISPLAY_BASE + 0xF0)
#define DISP_BLOCK_LEN  (DISPLAY_BASE + 0xF4)  // bytes (multiple of 4)
#define DISP_BLOCK_CMD  (DISPLAY_BASE + 0xF8)  // write: 2 blits pixels

#define DISP_CMD_CLEAR        1
#define DISP_CMD_PRESENT      2
#define DISP_BLOCK_CMD_WRITE  2

// Pack an RGB triple into a 0x00RRGGBB pixel.
#define RGB(r, g, b) (((unsigned int)(r) << 16) | ((unsigned int)(g) << 8) | (unsigned int)(b))

void display_clear(void);
void display_show(void);
void display_size(int width, int height);
void display_blit(const unsigned int* pixels, int width, int height);
