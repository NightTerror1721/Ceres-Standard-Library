// The 2D blitter. See ceres/blitter.h.
#include "ceres/blitter.h"

int blitter_available(void)
{
    return mmio_r32(BLIT_STATUS) != 0xFFFFFFFFu;         // an empty slot reads all ones
}

static int run(unsigned int command)
{
    mmio_w32(BLIT_CMD, command);
    return (mmio_r32(BLIT_STATUS) & 1u) == 0 ? 0 : -1;
}

static int surfaces(const void* dst, int dst_stride, const void* src, int src_stride, int w, int h)
{
    if (!blitter_available() || w <= 0 || h <= 0)
        return -1;
    mmio_w32(BLIT_DST, (unsigned int)dst);
    mmio_w32(BLIT_DST_STRIDE, (unsigned int)dst_stride);
    mmio_w32(BLIT_SRC, (unsigned int)src);
    mmio_w32(BLIT_SRC_STRIDE, (unsigned int)src_stride);
    mmio_w32(BLIT_WIDTH, (unsigned int)w);
    mmio_w32(BLIT_HEIGHT, (unsigned int)h);
    return 0;
}

int blitter_fill(unsigned int* dst, int dst_stride, int w, int h, unsigned int color)
{
    if (surfaces(dst, dst_stride, 0, 0, w, h) != 0)
        return -1;
    mmio_w32(BLIT_COLOR, color);
    return run(BLIT_FILL);
}

int blitter_copy(unsigned int* dst, int dst_stride, const unsigned int* src, int src_stride, int w, int h)
{
    if (surfaces(dst, dst_stride, src, src_stride, w, h) != 0)
        return -1;
    return run(BLIT_COPY);
}

int blitter_copy_key(unsigned int* dst, int dst_stride, const unsigned int* src, int src_stride, int w, int h, unsigned int key)
{
    if (surfaces(dst, dst_stride, src, src_stride, w, h) != 0)
        return -1;
    mmio_w32(BLIT_COLOR, key);
    return run(BLIT_COPY_KEYED);
}

int blitter_copy_scaled(unsigned int* dst, int dst_stride, const unsigned int* src, int src_stride, int w, int h, int scale)
{
    if (scale < 1 || scale > 8 || surfaces(dst, dst_stride, src, src_stride, w, h) != 0)
        return -1;
    mmio_w32(BLIT_SCALE, (unsigned int)scale);
    return run(BLIT_COPY_SCALED);
}

int blitter_copy_indexed(unsigned int* dst, int dst_stride, const unsigned char* src, int src_stride, int w, int h,
                         const unsigned int* palette, int key)
{
    if (palette == 0 || surfaces(dst, dst_stride, src, src_stride, w, h) != 0)
        return -1;
    mmio_w32(BLIT_PALETTE, (unsigned int)palette);
    if (key >= 0)
        mmio_w32(BLIT_COLOR, (unsigned int)key);
    return run(key >= 0 ? BLIT_COPY_INDEXED_KEYED : BLIT_COPY_INDEXED);
}

unsigned int blitter_last_pixels(void)
{
    return mmio_r32(BLIT_PIXELS);
}
