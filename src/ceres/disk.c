#include "ceres/disk.h"

// Selects a sector; the device answers with ERROR when there is no such sector.
static int select_sector(unsigned int sector)
{
    mmio_w32(DISK_SECTOR_REG, sector);
    return (mmio_r32(DISK_STATUS_REG) & DISK_ERROR) ? -1 : 0;
}

unsigned int disk_sectors(void)
{
    return mmio_r32(DISK_SECTOR_COUNT_REG);
}

static int transfer(unsigned int sector, void* buf, unsigned int command)
{
    if (buf == 0 || select_sector(sector) != 0)
        return -1;
    mmio_w32(DISK_BLOCK_ADDR, (unsigned int)buf);
    mmio_w32(DISK_BLOCK_LEN, DISK_SECTOR_SIZE);
    mmio_w32(DISK_BLOCK_CMD, command);
    return (mmio_r32(DISK_STATUS_REG) & DISK_ERROR) ? -1 : 0;
}

int disk_read(unsigned int sector, void* buf)
{
    return transfer(sector, buf, 1);
}

int disk_write(unsigned int sector, const void* buf)
{
    return transfer(sector, (void*)buf, 2);
}

int disk_read_n(unsigned int sector, void* buf, unsigned int count)
{
    unsigned char* p = (unsigned char*)buf;
    for (unsigned int i = 0; i < count; i++)
    {
        if (disk_read(sector + i, p + i * DISK_SECTOR_SIZE) != 0)
            return -1;
    }
    return 0;
}

int disk_write_n(unsigned int sector, const void* buf, unsigned int count)
{
    const unsigned char* p = (const unsigned char*)buf;
    for (unsigned int i = 0; i < count; i++)
    {
        if (disk_write(sector + i, p + i * DISK_SECTOR_SIZE) != 0)
            return -1;
    }
    return 0;
}

int disk_flush(void)
{
    mmio_w32(DISK_CMD_REG, 1);
    return (mmio_r32(DISK_STATUS_REG) & DISK_ERROR) ? -1 : 0;
}
