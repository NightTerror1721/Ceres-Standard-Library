#include "ceres/disk.h"

// Selects a sector; the device answers with ERROR when there is no such sector.
static int select_sector(unsigned int sector)
{
    mmio_w32(DISK_SECTOR_REG, sector);
    return (mmio_r32(DISK_STATUS_REG) & DISK_ERROR) ? -1 : 0;
}

static unsigned int cached_sectors;
static int sectors_known;

unsigned int disk_sectors(void)
{
    if (sectors_known)
        return cached_sectors;
    // The smallest sector number that is refused is the size. Sector numbers are accepted up to the size
    // and refused from there on, so a binary search over the whole 32-bit range finds it in 32 probes.
    unsigned int lo = 0;
    unsigned int hi = 0xFFFFFFFFu;
    if (select_sector(hi) == 0)
    {
        cached_sectors = hi;                    // even the last number is valid: cannot happen with real storage
    }
    else
    {
        while (lo < hi)
        {
            unsigned int mid = lo + (hi - lo) / 2u;
            if (select_sector(mid) == 0)
                lo = mid + 1u;
            else
                hi = mid;
        }
        cached_sectors = lo;
    }
    sectors_known = 1;
    select_sector(0);                           // leave the disk on a valid sector, if it has one
    return cached_sectors;
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
