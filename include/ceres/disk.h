#pragma once

#include "../ceres.h"

// Disk (0xFF020000): sectors of 512 bytes. See CeresASM docs/07-IO-Devices-and-Ports.md.
//
// One sector moves at a time. Without `--disk image.img` the disk still exists (64 sectors, 32 KiB) but
// lives only as long as the machine does; with it, a host file is behind it and writes are saved when
// the machine stops (disk_flush() saves them earlier). Selecting a sector past the end sets the ERROR
// bit, and every function below reports it as -1.
//
// The device has no register that says how big the disk is: disk_sectors() finds out by asking which
// sector numbers are accepted, and remembers.

#define DISK_STATUS_REG (DISK_BASE + 0x00)   // R: bit0 READY, bit1 ERROR
#define DISK_CMD_REG    (DISK_BASE + 0x04)   // W: 1 writes the image to the host file
#define DISK_SECTOR_REG (DISK_BASE + 0x08)   // RW: the sector the next transfer uses
#define DISK_BLOCK_ADDR (DISK_BASE + 0xF0)
#define DISK_BLOCK_LEN  (DISK_BASE + 0xF4)
#define DISK_BLOCK_CMD  (DISK_BASE + 0xF8)   // 1 reads the selected sector into RAM, 2 writes it

#define DISK_SECTOR_SIZE 512
#define DISK_READY       1
#define DISK_ERROR       2

unsigned int disk_sectors(void);                                    // how many sectors the disk has
int disk_read(unsigned int sector, void* buf);                      // 512 bytes; 0 on success, -1 on error
int disk_write(unsigned int sector, const void* buf);
int disk_read_n(unsigned int sector, void* buf, unsigned int count);      // `count` sectors, one after another
int disk_write_n(unsigned int sector, const void* buf, unsigned int count);   // stops at the first error
int disk_flush(void);                                               // save to the host file; 0 ok, -1 error
