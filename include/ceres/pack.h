#pragma once

#include "../stddef.h"
#include "../stdio.h"
#include "blockdev.h"
#include "gfx.h"

// Resource packs: a program's assets - images, levels, text, music - in one file that is read a piece at a time,
// from a cartridge in a peripheral port, a file, or memory. Each entry is stored whole or LZ4-compressed
// (ceres/lz.h) and carries a CRC-32 of its bytes, checked when it is read.
//
//   node tools/mkpack.js game.cart art/ levels/ readme.txt       (on the host: build it)
//   ceres run --cart 1=game.cart game.cres
//
//   struct pack p;
//   pack_open_port(&p, 1);
//   size_t size;
//   char* level = pack_load(&p, "levels/one.txt", &size);   // malloc'd, with a NUL after the bytes
//   struct gfx_surface* hero = pack_image(&p, "art/hero.qoi", RGB(255, 0, 255));   // image_free it
//   pack_close(&p);
//
// Only the directory is kept in memory; an entry is read when it is asked for, through one sector of cache.
//
// THE FORMAT (little-endian). A 16-byte header: "CPAK", a u16 version (1), a u16 count of entries, the u32 offset
// of the directory, a u32 of zero. The directory: count entries of 48 bytes - the name (up to 31 bytes, NUL-padded
// to 32), the u32 offset of the data, its u32 size, the u32 bytes stored (fewer than the size: an LZ4 block; the
// same: stored as it is) and the u32 CRC-32 of the size bytes. The data anywhere after. tools/mkpack.js pads
// the file to whole sectors, so it can be plugged in as a cartridge.
//
// Errors are -1 or NULL with errno: EINVAL for something that is not a pack (or a broken entry), ENOENT for a
// name it does not have, ENOSPC when an entry does not fit in the buffer given, EIO when the medium fails or an
// entry's bytes do not match its CRC, ENOMEM.

#define PACK_NAME_MAX 31

struct pack_entry
{
    char name[PACK_NAME_MAX + 1];
    unsigned int offset;
    unsigned int size;              // its bytes
    unsigned int stored;            // the bytes it takes in the pack
    unsigned int crc;
};

struct pack
{
    int kind;                       // where it is read from: memory, a file, a block device
    const unsigned char* memory;
    FILE* file;
    struct blockdev device;
    unsigned int length;            // the bytes there are to read
    int count;
    struct pack_entry* entries;
    unsigned int cached;            // the sector in `sector`, plus one (0: none)
    unsigned char sector[BLOCKDEV_SECTOR];
};

int  pack_open(struct pack* p, const struct blockdev* dev);
int  pack_open_port(struct pack* p, int port);             // what is plugged into a peripheral port
int  pack_open_file(struct pack* p, const char* path);     // any path fopen takes; kept open until pack_close
int  pack_open_memory(struct pack* p, const void* data, size_t size);   // not copied: keep it while the pack is open
void pack_close(struct pack* p);

int  pack_count(const struct pack* p);
const struct pack_entry* pack_entry_at(const struct pack* p, int i);   // NULL outside 0..count-1
const struct pack_entry* pack_find(const struct pack* p, const char* name);   // NULL (ENOENT) when it has none

long  pack_read(struct pack* p, const struct pack_entry* e, void* dst, size_t cap);   // its size, or -1
void* pack_load(struct pack* p, const char* name, size_t* size);   // malloc'd; size may be NULL
struct gfx_surface* pack_image(struct pack* p, const char* name, unsigned int transparent);   // ceres/image.h
