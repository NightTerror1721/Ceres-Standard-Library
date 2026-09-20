#include "ceres/dma.h"

void dma_copy_async(void* dst, const void* src, unsigned int n)
{
    mmio_w32(DMA_SRC, (unsigned int)src);
    mmio_w32(DMA_DST, (unsigned int)dst);
    mmio_w32(DMA_LEN, n);
    mmio_w32(DMA_CMD, DMA_CMD_START);       // clears DONE, sets BUSY
}

int dma_busy(void)
{
    return (mmio_r32(DMA_STATUS) & DMA_BUSY) != 0;
}

int dma_done(void)
{
    return (mmio_r32(DMA_STATUS) & DMA_DONE) != 0;
}

unsigned int dma_transferred(void)
{
    return mmio_r32(DMA_TRANSFERRED);
}

void dma_wait(void)
{
    while ((mmio_r32(DMA_STATUS) & DMA_BUSY) != 0)
    {
    }
}

unsigned int dma_copy(void* dst, const void* src, unsigned int n)
{
    if (n == 0)
        return 0;
    dma_copy_async(dst, src, n);
    while ((mmio_r32(DMA_STATUS) & DMA_DONE) == 0)
    {
    }
    return mmio_r32(DMA_TRANSFERRED);
}
