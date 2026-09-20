#include "ceres/display.h"

void display_clear(void)
{
    mmio_w32(DISP_CMD, DISP_CMD_CLEAR);
}

void display_show(void)
{
    mmio_w32(DISP_CMD, DISP_CMD_PRESENT);
}

int display_width(void)  { return (int)mmio_r32(DISP_WIDTH); }
int display_height(void) { return (int)mmio_r32(DISP_HEIGHT); }

int display_size(int width, int height)
{
    if (width < 1 || height < 1 || width > DISP_MAX_WIDTH || height > DISP_MAX_HEIGHT)
        return -1;
    mmio_w32(DISP_WIDTH, (unsigned int)width);
    mmio_w32(DISP_HEIGHT, (unsigned int)height);
    return (display_width() == width && display_height() == height) ? 0 : -1;
}

void display_blit(const unsigned int* pixels, int width, int height)
{
    if (pixels == 0 || width < 1 || height < 1)
        return;
    if (display_width() != width || display_height() != height)
    {
        if (display_size(width, height) != 0)
            return;                                  // the device refused that size: draw nothing
    }
    display_clear();                                 // rewinds the cursor to the first pixel
    mmio_w32(DISP_BLOCK_ADDR, (unsigned int)pixels);
    mmio_w32(DISP_BLOCK_LEN, (unsigned int)width * (unsigned int)height * 4u);
    mmio_w32(DISP_BLOCK_CMD, DISP_BLOCK_CMD_WRITE);
}

void display_put(unsigned int rgb)
{
    mmio_w32(DISP_DATA, rgb);
}
