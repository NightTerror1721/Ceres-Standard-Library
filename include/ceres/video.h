#pragma once

#include "../ceres.h"

// The GPU (0xFF400000): the screen and what it shows, for every video level (CeresASM plan/v2 SPEC 7). The screen
// has a resolution (640x480 when the machine starts, or the profile's largest when that is smaller) and is drawn
// again at every vertical blank, 60 times a second (or 50), from the GPU's planes: the background colour, the
// bitmap plane (level V1, ceres/fb.h) and the text plane in front (level V0, ceres/text.h; the terminal draws there).
//
//   video_set_level(1);                 // the bitmap plane as well as the text
//   video_set_resolution(320, 240);
//   for (;;) { ...draw...; video_present(); video_wait_present(); }
//
// TIME. The screen keeps the machine's time: a vertical blank every CpuClockHz / 60 cycles, the remainder carried,
// so a program paced by video_wait_vblank() runs the same on every run and every host.
//
// PRESENT. What a plane shows can change at any moment, and is drawn as it is at the next blank. A Present asks for
// the planes' new bases - a flip of the bitmap's buffers, another font - to be taken at the next blank, together;
// it is also the moment `ceres run --screen-log` and `--frames` record the screen.

#define GPU_ID            (GPU_BASE + 0x000)   // R: 0x55504743, "CGPU"
#define GPU_VERSION       (GPU_BASE + 0x004)   // R: major << 16 | minor
#define GPU_CAPS          (GPU_BASE + 0x008)   // R: bits 0-6 the levels there are, 8-15 the bitmap formats
#define GPU_VRAM_SIZE     (GPU_BASE + 0x00C)   // R: bytes of VRAM
#define GPU_CLOCK_HZ      (GPU_BASE + 0x010)   // R
#define GPU_MODE          (GPU_BASE + 0x014)   // RW: the video level
#define GPU_MAX_LEVEL     (GPU_BASE + 0x018)   // R: the highest the machine's profile allows
#define GPU_CONTROL       (GPU_BASE + 0x01C)   // RW: bit 0 the display on
#define GPU_STATUS        (GPU_BASE + 0x020)   // R: GPU_STATUS_*
#define GPU_IRQ_ENABLE    (GPU_BASE + 0x024)   // RW: bit 0 VBlank (32), 1 line (33), 2 copy (34), 3 fault (35)
#define GPU_IRQ_STATUS    (GPU_BASE + 0x028)   // write 1s to clear
#define GPU_FAULT_CODE    (GPU_BASE + 0x02C)
#define GPU_FAULT_ADDRESS (GPU_BASE + 0x030)
#define GPU_WIDTH         (GPU_BASE + 0x100)   // RW: pixels
#define GPU_HEIGHT        (GPU_BASE + 0x104)
#define GPU_REFRESH       (GPU_BASE + 0x108)   // R: 50 or 60
#define GPU_LINES_TOTAL   (GPU_BASE + 0x10C)   // R
#define GPU_VCOUNT        (GPU_BASE + 0x110)   // R: the line being scanned
#define GPU_LINE_COMPARE  (GPU_BASE + 0x114)   // RW
#define GPU_FRAME_COUNTER (GPU_BASE + 0x118)   // R: vertical blanks so far
#define GPU_BACKGROUND    (GPU_BASE + 0x11C)   // RW: 0x00RRGGBB behind every plane
#define GPU_PRESENT       (GPU_BASE + 0x120)   // W: 1

// The copy engine: moves or fills bytes of RAM or VRAM while the program goes on (ceres/fb.h uses it).
#define GPU_COPY_SRC      (GPU_BASE + 0x280)
#define GPU_COPY_DST      (GPU_BASE + 0x284)
#define GPU_COPY_LENGTH   (GPU_BASE + 0x288)
#define GPU_COPY_FILL     (GPU_BASE + 0x28C)   // the 32-bit pattern a fill repeats, aligned to the address
#define GPU_COPY_COMMAND  (GPU_BASE + 0x290)   // W: 1 copy, 2 fill
#define GPU_COPY_STATUS   (GPU_BASE + 0x294)   // R: bit 0 busy, bit 1 the last one faulted

#define GPU_STATUS_BUSY          0x01
#define GPU_STATUS_FAULT         0x02
#define GPU_STATUS_VBLANK        0x04
#define GPU_STATUS_FLIP_PENDING  0x08

#define VIDEO_TEXT     0   // V0: the text plane
#define VIDEO_BITMAP   1   // V1: and the bitmap plane

int  video_level(void);                     // the level now
int  video_set_level(int level);            // the level the GPU took: never above what it has or the profile allows
int  video_max_level(void);
int  video_width(void);
int  video_height(void);
int  video_set_resolution(int width, int height);   // 0 when the GPU took exactly that, -1 when it is out of range
int  video_refresh(void);                   // frames a second
unsigned int video_frame(void);             // vertical blanks since the start (wraps)
unsigned int video_vram_size(void);
void video_set_background(unsigned int rgb);

void video_wait_vblank(void);               // until the next vertical blank has come
void video_present(void);                   // the planes' new bases at the next blank
void video_wait_present(void);              // until that blank has come: a flip is done, the log has the screen

// The copy engine, waited for: `bytes` from `src` to `dst`, or `bytes` of `pattern` at `dst`. Either address may be
// RAM or VRAM. 0, or -1 when an address is outside both.
int  video_copy(void* dst, const void* src, unsigned int bytes);
int  video_fill(void* dst, unsigned int pattern, unsigned int bytes);
