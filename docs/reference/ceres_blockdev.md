# `<ceres/blockdev.h>`

Block devices: what CeresFS (ceres/fs.h) keeps its volumes on. A device moves 512-byte sectors; the machine has its internal disk (ceres/disk.h) and the media plugged into the peripheral ports (ceres/periph.h): memory sticks, read and write, and cartridges, read only. A program can describe a device of its own - a RAM disk, an image in a file - by filling the structure.

```c
#define BLOCKDEV_SECTOR 512

struct blockdev
{
    int (*read)(void* ctx, unsigned int sector, void* buf);           // one sector; 0, or -1 on an error
    int (*write)(void* ctx, unsigned int sector, const void* buf);    // one sector; 0, or -1 (also on a read-only device)
    unsigned int (*sectors)(void* ctx);                               // how many it has
    int (*flush)(void* ctx);                                          // what the device holds back, out; may be NULL
    void* ctx;
    int read_only;                                                    // 1: nothing is ever written (a cartridge)
};

// The machine's internal disk.
void blockdev_disk(struct blockdev* out);

// What is plugged into peripheral port `port`: 0 and `out` filled, or -1 when the port is empty or does not exist.
// A cartridge, or a stick the host write-protected, is read_only.
int blockdev_port(int port, struct blockdev* out);
```
