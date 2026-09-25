// Resource packs. See ceres/pack.h.
#include "ceres/pack.h"
#include "ceres/lz.h"
#include "ceres/hash.h"
#include "stdlib.h"
#include "string.h"
#include "errno.h"

enum { FROM_MEMORY = 1, FROM_FILE = 2, FROM_DEVICE = 3 };

#define HEADER 16
#define ENTRY  48

static int fail(int e)
{
    errno = e;
    return -1;
}

static unsigned int le16(const unsigned char* p) { return (unsigned int)p[0] | ((unsigned int)p[1] << 8); }
static unsigned int le32(const unsigned char* p) { return le16(p) | (le16(p + 2) << 16); }

// n bytes at `offset` of the pack into dst: 0, or -1.
static int fetch(struct pack* p, unsigned int offset, void* dst, size_t n)
{
    if (offset > p->length || n > p->length - offset)
        return fail(EINVAL);                         // past the end: the entry is broken
    unsigned char* out = (unsigned char*)dst;
    if (p->kind == FROM_MEMORY)
    {
        memcpy(out, p->memory + offset, n);
        return 0;
    }
    if (p->kind == FROM_FILE)
    {
        if (fseek(p->file, (long)offset, SEEK_SET) != 0 || fread(out, 1, n, p->file) != n)
            return fail(EIO);
        return 0;
    }
    while (n > 0)
    {
        unsigned int sector = offset / BLOCKDEV_SECTOR;
        unsigned int within = offset % BLOCKDEV_SECTOR;
        size_t piece = BLOCKDEV_SECTOR - within;
        if (piece > n)
            piece = n;
        if (within == 0 && piece == BLOCKDEV_SECTOR && ((unsigned int)out & 3u) == 0)
        {
            if (p->device.read(p->device.ctx, sector, out) != 0)   // a whole sector goes straight in
                return fail(EIO);
        }
        else
        {
            if (p->cached != sector + 1)
            {
                p->cached = 0;
                if (p->device.read(p->device.ctx, sector, p->sector) != 0)
                    return fail(EIO);
                p->cached = sector + 1;
            }
            memcpy(out, p->sector + within, piece);
        }
        out += piece;
        offset += (unsigned int)piece;
        n -= piece;
    }
    return 0;
}

// The header and the directory, once the source is set.
static int load_directory(struct pack* p)
{
    unsigned char header[HEADER];
    p->count = 0;
    p->entries = 0;
    p->cached = 0;
    if (fetch(p, 0, header, HEADER) != 0 || memcmp(header, "CPAK", 4) != 0 || le16(header + 4) != 1)
        return fail(errno == EIO ? EIO : EINVAL);
    int count = (int)le16(header + 6);
    unsigned int directory = le32(header + 8);
    if (count == 0)
        return 0;
    p->entries = (struct pack_entry*)malloc((size_t)count * sizeof(struct pack_entry));
    if (p->entries == 0)
        return fail(ENOMEM);
    for (int i = 0; i < count; i++)
    {
        unsigned char raw[ENTRY];
        if (fetch(p, directory + (unsigned int)i * ENTRY, raw, ENTRY) != 0)
        {
            int e = errno;
            free(p->entries);
            p->entries = 0;
            return fail(e == EIO ? EIO : EINVAL);
        }
        struct pack_entry* e = &p->entries[i];
        memcpy(e->name, raw, PACK_NAME_MAX);
        e->name[PACK_NAME_MAX] = 0;
        e->offset = le32(raw + 32);
        e->size = le32(raw + 36);
        e->stored = le32(raw + 40);
        e->crc = le32(raw + 44);
    }
    p->count = count;
    return 0;
}

int pack_open_memory(struct pack* p, const void* data, size_t size)
{
    memset(p, 0, sizeof *p);
    p->kind = FROM_MEMORY;
    p->memory = (const unsigned char*)data;
    p->length = size > 0xFFFFFFFFu ? 0xFFFFFFFFu : (unsigned int)size;
    return load_directory(p);
}

int pack_open(struct pack* p, const struct blockdev* dev)
{
    memset(p, 0, sizeof *p);
    if (dev == 0 || dev->read == 0 || dev->sectors == 0)
        return fail(EINVAL);
    p->kind = FROM_DEVICE;
    p->device = *dev;
    unsigned int sectors = dev->sectors(dev->ctx);
    p->length = sectors >= 0xFFFFFFFFu / BLOCKDEV_SECTOR ? 0xFFFFFFFFu : sectors * BLOCKDEV_SECTOR;
    return load_directory(p);
}

int pack_open_port(struct pack* p, int port)
{
    struct blockdev dev;
    if (blockdev_port(port, &dev) != 0)
    {
        memset(p, 0, sizeof *p);
        return fail(ENODEV);
    }
    return pack_open(p, &dev);
}

int pack_open_file(struct pack* p, const char* path)
{
    memset(p, 0, sizeof *p);
    FILE* f = fopen(path, "rb");
    if (f == 0)
        return -1;
    long end = -1;
    if (fseek(f, 0, SEEK_END) == 0)
        end = ftell(f);
    if (end < 0)
    {
        fclose(f);
        return fail(EINVAL);                         // a stream with no end to seek to
    }
    p->kind = FROM_FILE;
    p->file = f;
    p->length = (unsigned int)end;
    if (load_directory(p) != 0)
    {
        int e = errno;
        fclose(f);
        p->file = 0;
        return fail(e);
    }
    return 0;
}

void pack_close(struct pack* p)
{
    if (p == 0)
        return;
    free(p->entries);
    if (p->file != 0)
        fclose(p->file);
    memset(p, 0, sizeof *p);
}

int pack_count(const struct pack* p)
{
    return p != 0 ? p->count : 0;
}

const struct pack_entry* pack_entry_at(const struct pack* p, int i)
{
    return p != 0 && i >= 0 && i < p->count ? &p->entries[i] : 0;
}

const struct pack_entry* pack_find(const struct pack* p, const char* name)
{
    for (int i = 0; p != 0 && name != 0 && i < p->count; i++)
        if (strcmp(p->entries[i].name, name) == 0)
            return &p->entries[i];
    errno = ENOENT;
    return 0;
}

long pack_read(struct pack* p, const struct pack_entry* e, void* dst, size_t cap)
{
    if (p == 0 || e == 0 || e->stored > e->size || (e->size > 0 && e->stored == 0))
        return fail(EINVAL);
    if (e->size > cap)
        return fail(ENOSPC);
    if (e->stored == e->size)
    {
        if (fetch(p, e->offset, dst, e->size) != 0)
            return -1;
    }
    else
    {
        void* packed = malloc(e->stored);
        if (packed == 0)
            return fail(ENOMEM);
        int ok = fetch(p, e->offset, packed, e->stored) == 0;
        int e_saved = errno;
        int got = ok ? lz4_decompress(packed, e->stored, dst, e->size) : -1;
        free(packed);
        if (!ok)
            return fail(e_saved);
        if (got != (int)e->size)
            return fail(EINVAL);
    }
    if (hash_crc32(dst, e->size) != e->crc)
        return fail(EIO);
    return (long)e->size;
}

void* pack_load(struct pack* p, const char* name, size_t* size)
{
    const struct pack_entry* e = pack_find(p, name);
    if (e == 0)
        return 0;
    char* data = (char*)malloc((size_t)e->size + 1u);
    if (data == 0)
    {
        errno = ENOMEM;
        return 0;
    }
    if (pack_read(p, e, data, e->size) < 0)
    {
        int saved = errno;
        free(data);
        errno = saved;
        return 0;
    }
    data[e->size] = 0;                               // text can be used as a string
    if (size != 0)
        *size = e->size;
    return data;
}
