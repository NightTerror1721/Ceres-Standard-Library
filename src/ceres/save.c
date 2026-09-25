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

// Opens one copy and reads its header: 1 with *out open at its bytes when the header checks out, 0 when the copy is
// not there or its header does not, and -1 (errno) when it is there but could not be opened - which must not pass
// for "no copy", or a write would go over a good save that was only unreadable for a moment.
static int open_copy(const char* path, int which, struct header* h, FILE** out)
{
    char name[SAVE_PATH_MAX + 3];
    *out = 0;
    if (name_of(name, path, which) != 0)
        return -1;
    FILE* f = fopen(name, "rb");
    if (f == 0)
        return errno == ENOENT ? 0 : -1;
    unsigned char raw[HEADER];
    if (fread(raw, 1, HEADER, f) != HEADER || get32(raw) != MAGIC || get32(raw + 20) != hash_crc32(raw, 20))
    {
        fclose(f);
        return 0;
    }
    h->version = get32(raw + 4);
    h->sequence = get32(raw + 8);
    h->size = get32(raw + 12);
    h->crc = get32(raw + 16);
    *out = f;
    return 1;
}

// The bytes after a good header, a piece at a time through the CRC, into `data` when it is not NULL (it has room for
// them all): 1 when every one is there, the CRC matches and nothing follows.
static int read_body(FILE* f, const struct header* h, void* data)
{
    unsigned int crc = 0;
    unsigned char piece[128];
    unsigned int left = h->size;
    unsigned char* out = (unsigned char*)data;
    int ok = 1;
    while (ok && left > 0)
    {
        unsigned int n = left < sizeof piece ? left : (unsigned int)sizeof piece;
        unsigned char* into = out != 0 ? out : piece;
        ok = fread(into, 1, n, f) == n;
        if (ok)
            crc = hash_crc32_update(crc, into, n);
        if (out != 0)
            out += n;
        left -= n;
    }
    return ok && crc == h->crc && fgetc(f) == EOF;
}

// The newest good copy: both headers are read, the newer (by sequence number, which wraps round) is tried first and
// taken if its bytes check out, else the other. It returns 0 (a) or 1 (b) with its header in *h and, when data is
// not NULL, its bytes there - each copy's bytes are read at most once. -1 with ENOENT when no copy is good, ENOSPC
// when the good one does not fit in cap, or the error that kept a copy from being opened.
static int newest(const char* path, struct header* h, void* data, size_t cap)
{
    struct header heads[2];
    FILE* files[2] = { 0, 0 };
    int result = -1;
    int error = ENOENT;
    for (int which = 0; which < 2 && error == ENOENT; which++)
        if (open_copy(path, which, &heads[which], &files[which]) < 0)
            error = errno;
    if (error == ENOENT)
    {
        int first = 0;
        if (files[0] == 0 || (files[1] != 0 && heads[1].sequence - heads[0].sequence < 0x80000000u))
            first = 1;
        for (int k = 0; k < 2 && result < 0; k++)
        {
            int which = k == 0 ? first : 1 - first;
            if (files[which] == 0)
                continue;
            if (data != 0 && heads[which].size > cap)
            {
                if (read_body(files[which], &heads[which], 0))
                {
                    error = ENOSPC;                                  // good, and too big: not the older one instead
                    break;
                }
                continue;
            }
            if (read_body(files[which], &heads[which], data))
            {
                *h = heads[which];
                result = which;
            }
        }
    }
    for (int which = 0; which < 2; which++)
        if (files[which] != 0)
            fclose(files[which]);
    if (result < 0)
        errno = error;
    return result;
}

int save_write(const char* path, unsigned int version, const void* data, size_t size)
{
    struct header latest_header;
    int saved_errno = errno;
    int latest = newest(path, &latest_header, 0, 0);
    if (latest < 0 && errno != ENOENT)
        return -1;
    errno = saved_errno;
    int which = latest == 0 ? 1 : 0;                                 // over the older one, or a when there is none
    unsigned int sequence = latest >= 0 ? latest_header.sequence + 1u : 1u;
    char name[SAVE_PATH_MAX + 3];
    if (name_of(name, path, which) != 0)
        return -1;
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
    struct header h;
    if (newest(path, &h, data, cap) < 0)
        return -1;
    if (version != 0)
        *version = h.version;
    return (long)h.size;
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
    long got = save_read(path, version, data, (size_t)n);            // (a newer save since would not fit: ENOSPC)
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
    struct header h;
    return newest(path, &h, 0, 0) < 0 ? -1 : (long)h.size;
}

int save_exists(const char* path)
{
    struct header h;
    int saved = errno;
    int yes = newest(path, &h, 0, 0) >= 0;
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
        int saved = errno;
        if (remove(name) != 0)
        {
            if (errno == ENOENT)
                errno = saved;                                       // not there: nothing to erase
            else
                result = -1;
        }
    }
    return result;
}
