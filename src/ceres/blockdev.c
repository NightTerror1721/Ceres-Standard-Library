// Block devices over the internal disk and the peripheral ports. See ceres/blockdev.h.
#include "ceres/blockdev.h"
#include "ceres/disk.h"
#include "ceres/periph.h"

static int disk_rd(void* ctx, unsigned int sector, void* buf)         { return disk_read(sector, buf); }
static int disk_wr(void* ctx, unsigned int sector, const void* buf)   { return disk_write(sector, buf); }
static unsigned int disk_count(void* ctx)                             { return disk_sectors(); }
static int disk_fl(void* ctx)                                         { return disk_flush(); }

void blockdev_disk(struct blockdev* out)
{
    out->read = disk_rd;
    out->write = disk_wr;
    out->sectors = disk_count;
    out->flush = disk_fl;
    out->ctx = 0;
    out->read_only = 0;
}

// The port number travels in ctx.
static int port_of(void* ctx)                                         { return (int)(unsigned int)ctx; }
static int port_rd(void* ctx, unsigned int sector, void* buf)         { return periph_read(port_of(ctx), sector, buf); }
static int port_wr(void* ctx, unsigned int sector, const void* buf)   { return periph_write(port_of(ctx), sector, buf); }
static unsigned int port_count(void* ctx)                             { return periph_sectors(port_of(ctx)); }
static int port_fl(void* ctx)                                         { return periph_flush(port_of(ctx)); }

int blockdev_port(int port, struct blockdev* out)
{
    if (!periph_present(port))
        return -1;
    out->read = port_rd;
    out->write = port_wr;
    out->sectors = port_count;
    out->flush = port_fl;
    out->ctx = (void*)(unsigned int)port;
    out->read_only = periph_protected(port);
    return 0;
}
