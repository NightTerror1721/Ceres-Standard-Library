// Saved games in two alternating copies. See ceres/save.h.
#include "ceres/save.h"
#include "ceres/hash.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "errno.h"

// A copy is this header and then the bytes. The header's own CRC covers its first 20 bytes, so a torn header is
// caught before the size in it is believed.
#define HEADER 24
#define MAGIC  0x56415343u               // "CSAV"

struct header
{
    unsigned int version, sequence, size, crc;
};

struct copy
{
    int good;
    struct header h;
};

static void put32(unsigned char* p, unsigned int v)
{
    for (int i = 0; i < 4; i++)
        p[i] = (unsigned char)(v >> (8 * i));
}

static unsigned int get32(const unsigned char* p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

// "<path>.a" or ".b" into name: 0, or -1 (ENAMETOOLONG).
static int name_of(char* name, const char* path, int which)
{
    size_t length = path != 0 ? strlen(path) : 0;
    if (length == 0 || length > SAVE_PATH_MAX)
    {
        errno = length == 0 ? EINVAL : ENAMETOOLONG;
        return -1;
    }
    memcpy(name, path, length);
    name[length] = '.';
    name[length + 1] = (char)('a' + which);
    name[length + 2] = 0;
    return 0;
}

// One copy's header, and whether its bytes check out. With `data`, the bytes go there too: a copy larger than `cap`
// is not read, and not good.
static int examine(const char* path, int which, struct copy* c, void* data, size_t cap)
{
    char name[SAVE_PATH_MAX + 3];
    c->good = 0;
    if (name_of(name, path, which) != 0)
        return -1;
    FILE* f = fopen(name, "rb");
    if (f == 0)
        return 0;
    unsigned char raw[HEADER];
    int ok = fread(raw, 1, HEADER, f) == HEADER && get32(raw) == MAGIC && get32(raw + 20) == hash_crc32(raw, 20);
    if (ok)
    {
        c->h.version = get32(raw + 4);
        c->h.sequence = get32(raw + 8);
        c->h.size = get32(raw + 12);
        c->h.crc = get32(raw + 16);
        if (data != 0 && c->h.size > cap)
            ok = 0;
        // The bytes, a piece at a time through the CRC, into `data` when there is somewhere to put them.
        unsigned int crc = 0;
        unsigned char piece[128];
        unsigned int left = c->h.size;
        unsigned char* out = (unsigned char*)data;
        while (ok && left > 0)
        {
            unsigned int n = left < sizeof piece ? left : (unsigned int)sizeof piece;
            unsigned char* into = out != 0 ? out : piece;
            ok = fread(into, 1, n, f) == n;
            crc = hash_crc32_update(crc, into, n);
            if (out != 0)
                out += n;
            left -= n;
        }
        ok = ok && crc == c->h.crc && fgetc(f) == EOF;           // nothing after it either
    }
    fclose(f);
    c->good = ok;
    return 0;
}

// Which copy is the newest good one: 0 (a), 1 (b), or -1 with ENOENT.
static int newest(const char* path, struct copy copies[2])
{
    if (examine(path, 0, &copies[0], 0, 0) != 0 || examine(path, 1, &copies[1], 0, 0) != 0)
        return -1;
    if (copies[0].good && copies[1].good)
        return copies[1].h.sequence - copies[0].h.sequence < 0x80000000u ? 1 : 0;   // the sequence wraps round
    if (copies[0].good)
        return 0;
    if (copies[1].good)
        return 1;
    errno = ENOENT;
    return -1;
}

int save_write(const char* path, unsigned int version, const void* data, size_t size)
{
    struct copy copies[2];
    int saved_errno = errno;
    int latest = newest(path, copies);
    if (latest < 0 && errno != ENOENT)
        return -1;
    errno = saved_errno;
    int which = latest == 0 ? 1 : 0;                                 // over the older one, or a when there is none
    unsigned int sequence = latest >= 0 ? copies[latest].h.sequence + 1u : 1u;
    char name[SAVE_PATH_MAX + 3];
    name_of(name, path, which);
    unsigned char raw[HEADER];
    put32(raw, MAGIC);
    put32(raw + 4, version);
    put32(raw + 8, sequence);
    put32(raw + 12, (unsigned int)size);
    put32(raw + 16, hash_crc32(data, size));
    put32(raw + 20, hash_crc32(raw, 20));
    FILE* f = fopen(name, "wb");
    if (f == 0)
        return -1;
    int ok = fwrite(raw, 1, HEADER, f) == HEADER && (size == 0 || fwrite(data, 1, size, f) == size);
    if (fclose(f) != 0)                                              // the disk has it once the file is closed
        ok = 0;
    if (!ok)
    {
        errno = EIO;
        return -1;
    }
    return 0;
}

long save_read(const char* path, unsigned int* version, void* data, size_t cap)
{
    struct copy copies[2];
    int latest = newest(path, copies);
    if (latest < 0)
        return -1;
    if (copies[latest].h.size > cap)
    {
        errno = ENOSPC;
        return -1;
    }
    // Read again, into data this time: had the file changed since, its CRC would say so.
    struct copy again;
    if (examine(path, latest, &again, data, cap) != 0)
        return -1;
    if (!again.good)
    {
        errno = EIO;
        return -1;
    }
    if (version != 0)
        *version = again.h.version;
    return (long)again.h.size;
}

void* save_load(const char* path, unsigned int* version, size_t* size)
{
    long n = save_size(path);
    if (n < 0)
        return 0;
    void* data = malloc(n > 0 ? (size_t)n : 1u);
    if (data == 0)
    {
        errno = ENOMEM;
        return 0;
    }
    long got = save_read(path, version, data, (size_t)n);
    if (got < 0)
    {
        int e = errno;
        free(data);
        errno = e;
        return 0;
    }
    if (size != 0)
        *size = (size_t)got;
    return data;
}

long save_size(const char* path)
{
    struct copy copies[2];
    int latest = newest(path, copies);
    return latest < 0 ? -1 : (long)copies[latest].h.size;
}

int save_exists(const char* path)
{
    struct copy copies[2];
    int saved = errno;
    int yes = newest(path, copies) >= 0;
    errno = saved;
    return yes;
}

int save_erase(const char* path)
{
    char name[SAVE_PATH_MAX + 3];
    int result = 0;
    for (int which = 0; which < 2; which++)
    {
        if (name_of(name, path, which) != 0)
            return -1;
        FILE* f = fopen(name, "rb");
        if (f == 0)
            continue;                                                // not there: nothing to erase
        fclose(f);
        if (remove(name) != 0)
            result = -1;
    }
    return result;
}
