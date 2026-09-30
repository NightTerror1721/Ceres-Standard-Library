// The GPU's screen. See ceres/video.h.
#include "ceres/video.h"

int video_level(void) { return (int)mmio_r32(GPU_MODE); }

int video_set_level(int level)
{
    mmio_w32(GPU_MODE, (unsigned int)(level < 0 ? 0 : level));
    return video_level();
}

int video_max_level(void) { return (int)mmio_r32(GPU_MAX_LEVEL); }
int video_width(void) { return (int)mmio_r32(GPU_WIDTH); }
int video_height(void) { return (int)mmio_r32(GPU_HEIGHT); }
int video_refresh(void) { return (int)mmio_r32(GPU_REFRESH); }
unsigned int video_frame(void) { return mmio_r32(GPU_FRAME_COUNTER); }
unsigned int video_vram_size(void) { return mmio_r32(GPU_VRAM_SIZE); }
void video_set_background(unsigned int rgb) { mmio_w32(GPU_BACKGROUND, rgb & 0x00FFFFFFu); }

// The GPU clamps what it cannot show; the size it took is read back.
int video_set_resolution(int width, int height)
{
    if (width <= 0 || height <= 0)
        return -1;
    mmio_w32(GPU_WIDTH, (unsigned int)width);
    mmio_w32(GPU_HEIGHT, (unsigned int)height);
    return video_width() == width && video_height() == height ? 0 : -1;
}

// A spin on the frame counter: the blank is an event of the machine's clock, which the loop's own cycles move
// towards, so the wait is exact and the same on every run.
void video_wait_vblank(void)
{
    unsigned int frame = video_frame();
    while (video_frame() == frame)
    {
    }
}

void video_present(void)
{
    mmio_w32(GPU_PRESENT, 1u);
}

void video_wait_present(void)
{
    while (mmio_r32(GPU_STATUS) & GPU_STATUS_FLIP_PENDING)
    {
    }
}

static int copy_engine(unsigned int command, void* dst, unsigned int src_or_pattern, unsigned int bytes)
{
    if (bytes == 0)
        return 0;
    while (mmio_r32(GPU_COPY_STATUS) & 1u)
    {
    }
    mmio_w32(GPU_COPY_DST, (unsigned int)dst);
    mmio_w32(GPU_COPY_LENGTH, bytes);
    if (command == 1u)
        mmio_w32(GPU_COPY_SRC, src_or_pattern);
    else
        mmio_w32(GPU_COPY_FILL, src_or_pattern);
    mmio_w32(GPU_COPY_COMMAND, command);
    unsigned int status;
    while ((status = mmio_r32(GPU_COPY_STATUS)) & 1u)
    {
    }
    return (status & 2u) ? -1 : 0;
}

int video_copy(void* dst, const void* src, unsigned int bytes)
{
    return copy_engine(1u, dst, (unsigned int)src, bytes);
}

int video_fill(void* dst, unsigned int pattern, unsigned int bytes)
{
    return copy_engine(2u, dst, pattern, bytes);
}

// ---- the program's video memory ----
// It starts past the text plane's scrollback ring, the last thing the GPU puts in the VRAM when it starts (CeresASM
// plan/v2 SPEC 7.4): a quarter of the VRAM, no more than 256 KiB, no more than what is left.

#define ALIGN256(n) (((n) + 255u) & ~255u)

static unsigned int vram_next;   // 0 until the first allocation
static unsigned int vram_resets;

static unsigned int vram_start(void)
{
    const unsigned int size = video_vram_size();
    const unsigned int scrollback = mmio_r32(GPU_BASE + 0x22C);
    const unsigned int used = scrollback - VRAM_BASE;
    unsigned int ring = size / 4u;
    if (ring > 256u * 1024u)
        ring = 256u * 1024u;
    if (ring > size - used)
        ring = size - used;
    return ALIGN256(scrollback + ring);
}

static unsigned int vram_end(void) { return VRAM_BASE + video_vram_size(); }

void* video_vram_alloc(unsigned int bytes)
{
    if (vram_next == 0)
        vram_next = vram_start();
    const unsigned int rounded = ALIGN256(bytes);
    if (bytes == 0 || rounded < bytes || rounded > vram_end() - vram_next)
        return NULL;
    void* block = (void*)vram_next;
    vram_next += rounded;
    return block;
}

void video_vram_reset(void)
{
    vram_next = 0;
    ++vram_resets;
}

unsigned int video_vram_resets(void) { return vram_resets; }

unsigned int video_vram_free(void)
{
    if (vram_next == 0)
        vram_next = vram_start();
    return vram_end() - vram_next;
}
