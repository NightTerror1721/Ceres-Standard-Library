# `<ceres/display.h>`

Display device (0xFF070000): a pixel framebuffer of RGB32 pixels (0x00RRGGBB, top byte ignored). Distinct from the text framebuffer. A presented frame reaches the host through `ceres run --window` (SDL3) or a registered frame sink; without one, presenting is a no-op, so a headless program keeps working (and can be tested). See CeresASM docs/07-IO-Devices-and-Ports.md.

The device has no vertical sync: with a window the host runs the machine in slices of instructions and shows the frame between slices, so the pace of a game is set by how many instructions it spends per frame. Pixels are written sequentially from a cursor that only clear or a size change rewinds, which is why display_blit() always clears first and takes a whole frame.

```c
#define DISP_CMD        (DISPLAY_BASE + 0x00)  // write: 1 clear, 2 present
#define DISP_WIDTH      (DISPLAY_BASE + 0x04)  // rw: pixel columns, <= 1280
#define DISP_HEIGHT     (DISPLAY_BASE + 0x08)  // rw: pixel rows, <= 720
#define DISP_DATA       (DISPLAY_BASE + 0x0C)  // write: one pixel per word (sequential)
#define DISP_MODE       (DISPLAY_BASE + 0x10)  // rw: 0 RGB32, 1 indexed (a byte a pixel, through the palette)
#define DISP_PAL_INDEX  (DISPLAY_BASE + 0x14)  // write: the palette entry DISP_PAL_DATA sets next
#define DISP_PAL_DATA   (DISPLAY_BASE + 0x18)  // write: RGB32 for it, then the next
#define DISP_SCROLL_X   (DISPLAY_BASE + 0x1C)  // rw: the column shown at the left edge (wrapping)
#define DISP_SCROLL_Y   (DISPLAY_BASE + 0x20)  // rw: the row shown at the top
#define DISP_BLOCK_ADDR (DISPLAY_BASE + 0xF0)
#define DISP_BLOCK_LEN  (DISPLAY_BASE + 0xF4)  // bytes (multiple of 4)
#define DISP_BLOCK_CMD  (DISPLAY_BASE + 0xF8)  // write: 2 blits pixels

#define DISP_MODE_RGB32       0
#define DISP_MODE_INDEXED     1
#define DISP_CMD_CLEAR        1
#define DISP_CMD_PRESENT      2
#define DISP_BLOCK_CMD_WRITE  2
#define DISP_MAX_WIDTH        1280
#define DISP_MAX_HEIGHT       720

// Pack an RGB triple into a 0x00RRGGBB pixel.
#define RGB(r, g, b) (((unsigned int)(r) << 16) | ((unsigned int)(g) << 8) | (unsigned int)(b))

void display_clear(void);                       // paints the surface black and rewinds the write cursor
void display_show(void);                        // presents the frame
int  display_size(int width, int height);       // 0 when the device took it (this clears it), -1 when it refused
int  display_width(void);
int  display_height(void);

// Draws a whole frame: sets the size when it differs, clears, and sends width*height pixels in ONE block
// transfer. Does not present: call display_show() afterwards.
void display_blit(const unsigned int* pixels, int width, int height);

// Sequential single pixels from the cursor (after display_clear()). Slower than a blit; for a few pixels.
void display_put(unsigned int rgb);

// The retro touches (CeresASM 3b2db3b). In the indexed mode a pixel is a byte, an index into a palette of 256
// RGB32 colours: a quarter of the memory, and a colour changed in the palette changes everywhere at once. The
// scroll shifts what is presented, wrapping round, so a background can move without being drawn again.
int  display_set_mode(int mode);                                // DISP_MODE_RGB32 or DISP_MODE_INDEXED; 0, or -1 without the mode
void display_set_palette(int index, unsigned int rgb);
void display_load_palette(const unsigned int* colors, int count);   // entries 0..count-1
void display_blit8(const unsigned char* pixels, int width, int height);   // display_blit for the indexed mode
void display_scroll(int x, int y);                              // taken modulo the size; negatives count from the other side
```
