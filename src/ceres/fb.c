// The GPU's bitmap plane. See ceres/fb.h.
#include "ceres/fb.h"
#include "ceres/text.h"

static int width;
static int height;
static int indexed;
static unsigned int block;          // the plane's video memory: its palette, then the two buffers
static unsigned int block_bytes;
static unsigned int block_resets;   // video_vram_resets() when it was taken

#define ALIGN256(n) (((n) + 255u) & ~255u)

static unsigned int bytes_per_frame(void)
{
    return (unsigned int)width * (unsigned int)height * (indexed ? 1u : 4u);
}

// The program's video memory (video_vram_alloc): the palette first, then the two buffers. A later fb_init that needs
// no more than the first reuses its memory.
static int init(int w, int h, int bytes_per_pixel)
{
    if (w <= 0 || h <= 0)
        return -1;
    if (video_set_resolution(w, h) != 0)
        return -1;
    if (video_level() < VIDEO_BITMAP && video_set_level(VIDEO_BITMAP) < VIDEO_BITMAP)
        return -1;

    const unsigned int frame = ALIGN256((unsigned int)w * (unsigned int)h * (unsigned int)bytes_per_pixel);
    const unsigned int need = 1024u + 2u * frame;
    if (need < frame)
        return -1;
    if (need > block_bytes || block_resets != video_vram_resets())
    {
        void* got = video_vram_alloc(need);
        if (got == (void*)0)
            return -1;
        block = (unsigned int)got;
        block_bytes = need;
        block_resets = video_vram_resets();
    }
    const unsigned int palette = block;
    const unsigned int front = palette + 1024u;
    const unsigned int back = front + frame;

    width = w;
    height = h;
    indexed = bytes_per_pixel == 1;
    // The palette starts as the text's (the ANSI colours, xterm's cube and greys); both buffers black.
    video_copy((void*)palette, (const void*)mmio_r32(TEXT_PALETTE_BASE), 1024u);
    video_fill((void*)front, 0u, frame);
    video_fill((void*)back, 0u, frame);

    mmio_w32(FB_ENABLE, 0u);
    mmio_w32(FB_FORMAT, indexed ? FB_FORMAT_I8 : FB_FORMAT_XRGB8888);
    mmio_w32(FB_PLANE_WIDTH, (unsigned int)w);
    mmio_w32(FB_PLANE_HEIGHT, (unsigned int)h);
    mmio_w32(FB_PITCH, (unsigned int)w * (unsigned int)bytes_per_pixel);
    mmio_w32(FB_SCROLL_X, 0u);
    mmio_w32(FB_SCROLL_Y, 0u);
    mmio_w32(FB_BUFFERS, 1u);                 // one while the bases are set, so the Present below flips nothing
    mmio_w32(FB_BASE, front);
    mmio_w32(FB_BACK_BASE, back);
    mmio_w32(FB_PALETTE_BASE, palette);
    video_present();                          // the new bases take effect at the blank
    video_wait_present();
    mmio_w32(FB_BUFFERS, 2u);
    mmio_w32(FB_ENABLE, 1u);
    return 0;
}

int fb_init(int w, int h) { return init(w, h, 4); }
int fb_init_indexed(int w, int h) { return init(w, h, 1); }

void fb_shutdown(void)
{
    mmio_w32(FB_ENABLE, 0u);
    if (video_level() == VIDEO_BITMAP)
        video_set_level(VIDEO_TEXT);
    width = height = indexed = 0;
}

int fb_width(void) { return width; }
int fb_height(void) { return height; }
int fb_indexed(void) { return indexed; }

void* fb_pixels(void)
{
    return width > 0 ? (void*)mmio_r32(FB_BACK_BASE) : (void*)0;
}

void fb_present(void)
{
    if (width <= 0)
        return;
    video_present();
    video_wait_present();
}

void fb_keep(void)
{
    if (width > 0)
        video_copy((void*)mmio_r32(FB_BACK_BASE), (const void*)mmio_r32(FB_BASE), bytes_per_frame());
}

void fb_clear(unsigned int value)
{
    if (width <= 0)
        return;
    const unsigned int pattern = indexed ? (value & 0xFFu) * 0x01010101u : value;
    video_fill(fb_pixels(), pattern, bytes_per_frame());
}

void fb_blit(const void* pixels)
{
    if (width > 0 && pixels != 0)
        video_copy(fb_pixels(), pixels, bytes_per_frame());
}

void fb_set_palette(int index, unsigned int rgb)
{
    if (width <= 0)
        return;
    unsigned int* palette = (unsigned int*)mmio_r32(FB_PALETTE_BASE);
    palette[index & 255] = rgb & 0x00FFFFFFu;
}

void fb_load_palette(const unsigned int* colors, int count)
{
    if (colors == 0)
        return;
    for (int i = 0; i < count && i < 256; i++)
        fb_set_palette(i, colors[i]);
}

void fb_scroll(int x, int y)
{
    if (width <= 0)
        return;
    x %= width;
    y %= height;
    mmio_w32(FB_SCROLL_X, (unsigned int)(x < 0 ? x + width : x));
    mmio_w32(FB_SCROLL_Y, (unsigned int)(y < 0 ? y + height : y));
}
