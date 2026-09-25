// LZ4 blocks and frames. See ceres/lz.h.
#include "ceres/lz.h"
#include "string.h"
#include "errno.h"

typedef unsigned char u8;

#define MIN_MATCH      4
#define MF_LIMIT       12        // the last match starts at least this far from the end
#define LAST_LITERALS  5         // ... and the last five bytes are literals
#define HASH_BITS      12

static int fail(int e)
{
    errno = e;
    return -1;
}

// ---- decompression ----

// One block into base[pos..cap): a match may reach back into base[0..pos), what came before it (a linked block of
// a frame). The bytes written, or -1.
static int decode(const u8* ip, size_t n, u8* base, size_t pos, size_t cap)
{
    const u8* end = ip + n;
    size_t op = pos;
    for (;;)
    {
        if (ip >= end)
            return fail(EINVAL);
        unsigned int token = *ip++;
        size_t literals = token >> 4;
        if (literals == 15)
        {
            unsigned int b;
            do
            {
                if (ip >= end)
                    return fail(EINVAL);
                b = *ip++;
                literals += b;
            } while (b == 255 && literals <= n);
        }
        if (literals > (size_t)(end - ip))
            return fail(EINVAL);
        if (literals > cap - op)
            return fail(ENOSPC);
        memcpy(base + op, ip, literals);
        ip += literals;
        op += literals;
        if (ip == end)
            return (int)(op - pos);                  // the last sequence: literals and no match
        if (end - ip < 2)
            return fail(EINVAL);
        size_t offset = (size_t)ip[0] | ((size_t)ip[1] << 8);
        ip += 2;
        if (offset == 0 || offset > op)
            return fail(EINVAL);                     // before anything written
        size_t length = token & 15u;
        if (length == 15)
        {
            unsigned int b;
            do
            {
                if (ip >= end)
                    return fail(EINVAL);
                b = *ip++;
                length += b;
            } while (b == 255 && length <= cap);
        }
        length += MIN_MATCH;
        if (length > cap - op)
            return fail(ENOSPC);
        const u8* from = base + op - offset;
        u8* to = base + op;
        for (size_t i = 0; i < length; i++)          // byte by byte: a match may overlap what it writes
            to[i] = from[i];
        op += length;
    }
}

int lz4_decompress(const void* src, size_t n, void* dst, size_t cap)
{
    return decode((const u8*)src, n, (u8*)dst, 0, cap);
}

static unsigned int le32(const u8* p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

#define FRAME_MAGIC      0x184D2204u
#define SKIPPABLE_MASK   0xFFFFFFF0u
#define SKIPPABLE_MAGIC  0x184D2A50u

// The frame header at p: its length, with the flags, or -1.
static int frame_header(const u8* p, size_t n, unsigned int* flags, long long* content)
{
    if (n < 7 || le32(p) != FRAME_MAGIC)
        return -1;
    unsigned int flg = p[4];
    if ((flg >> 6) != 1 || (flg & 0x02u) != 0 || (p[5] & 0x8Fu) != 0)
        return -1;                                   // version 01, reserved bits clear
    unsigned int bd = (p[5] >> 4) & 7u;
    if (bd < 4)
        return -1;                                   // the block sizes are 64 KiB (4) to 4 MiB (7)
    size_t length = 7;
    *content = -1;
    if (flg & 0x08u)
    {
        if (n < length + 8)
            return -1;
        unsigned long long size = (unsigned long long)le32(p + 6) | ((unsigned long long)le32(p + 10) << 32);
        *content = (long long)size;
        length += 8;
    }
    if (flg & 0x01u)
        length += 4;                                 // a dictionary id
    if (n < length)
        return -1;
    *flags = flg;
    return (int)length;
}

long long lz4_frame_content_size(const void* src, size_t n)
{
    unsigned int flags;
    long long content;
    if (frame_header((const u8*)src, n, &flags, &content) < 0)
        return -1;
    return content;
}

int lz4_frame_decompress(const void* src, size_t n, void* dst, size_t cap)
{
    const u8* p = (const u8*)src;
    const u8* end = p + n;
    u8* out = (u8*)dst;
    size_t written = 0;
    int frames = 0;
    while (p < end)
    {
        if (end - p >= 8 && (le32(p) & SKIPPABLE_MASK) == SKIPPABLE_MAGIC)
        {
            unsigned int size = le32(p + 4);         // a skippable frame: data for someone else
            if (size > (size_t)(end - p) - 8)
                return fail(EINVAL);
            p += 8 + size;
            continue;
        }
        unsigned int flags;
        long long content;
        int header = frame_header(p, (size_t)(end - p), &flags, &content);
        if (header < 0)
            return frames > 0 ? (int)written : fail(EINVAL);   // what follows the last frame is not ours
        if (flags & 0x01u)
            return fail(EINVAL);                     // a frame compressed against a dictionary we do not have
        p += header;
        size_t frame_start = written;
        int independent = (flags & 0x20u) != 0;
        int block_checksum = (flags & 0x10u) != 0;
        for (;;)
        {
            if (end - p < 4)
                return fail(EINVAL);
            unsigned int size = le32(p);
            p += 4;
            if (size == 0)
                break;                               // the end mark
            int stored = (size & 0x80000000u) != 0;
            size &= 0x7FFFFFFFu;
            if (size > (size_t)(end - p) || (block_checksum && (size_t)(end - p) - size < 4))
                return fail(EINVAL);
            if (stored)
            {
                if (size > cap - written)
                    return fail(ENOSPC);
                memcpy(out + written, p, size);
                written += size;
            }
            else
            {
                // A linked block may copy from the blocks of its frame before it; an independent one from none.
                u8* base = independent ? out + written : out + frame_start;
                size_t pos = independent ? 0 : written - frame_start;
                int got = decode(p, size, base, pos, independent ? cap - written : cap - frame_start);
                if (got < 0)
                    return -1;
                written += (size_t)got;
            }
            p += size + (block_checksum ? 4 : 0);
        }
        if (flags & 0x04u)
        {
            if (end - p < 4)
                return fail(EINVAL);
            p += 4;                                  // the content checksum
        }
        frames++;
    }
    if (frames == 0)
        return fail(EINVAL);
    return (int)written;
}

// ---- compression ----

static unsigned int table[1u << HASH_BITS];      // position + 1 of the last 4 bytes with each hash; 0 is none

static unsigned int read32(const u8* p)
{
    unsigned int v;
    memcpy(&v, p, 4);
    return v;
}

static unsigned int hash(unsigned int v)
{
    return (v * 2654435761u) >> (32 - HASH_BITS);
}

size_t lz4_bound(size_t n)
{
    return LZ4_BOUND(n);
}

// A length of 15 or more goes on in bytes of 255 and a last one below it.
static int put_length(u8* dst, size_t* op, size_t cap, size_t extra)
{
    while (extra >= 255)
    {
        if (*op >= cap)
            return -1;
        dst[(*op)++] = 255;
        extra -= 255;
    }
    if (*op >= cap)
        return -1;
    dst[(*op)++] = (u8)extra;
    return 0;
}

// Literals src[anchor..at) and then, with a length, a match `offset` back: one sequence.
static int put_sequence(const u8* src, size_t anchor, size_t at, size_t offset, size_t length,
                        u8* dst, size_t* op, size_t cap)
{
    size_t literals = at - anchor;
    if (*op >= cap)
        return -1;
    size_t token = *op;
    (*op)++;
    unsigned int t = (unsigned int)(literals >= 15 ? 15 : literals) << 4;
    if (literals >= 15 && put_length(dst, op, cap, literals - 15) != 0)
        return -1;
    if (literals > cap - *op)
        return -1;
    memcpy(dst + *op, src + anchor, literals);
    *op += literals;
    if (length > 0)
    {
        if (cap - *op < 2)
            return -1;
        dst[(*op)++] = (u8)(offset & 0xFF);
        dst[(*op)++] = (u8)(offset >> 8);
        size_t extra = length - MIN_MATCH;
        t |= extra >= 15 ? 15u : (unsigned int)extra;
        if (extra >= 15 && put_length(dst, op, cap, extra - 15) != 0)
            return -1;
    }
    dst[token] = (u8)t;
    return 0;
}

int lz4_compress(const void* source, size_t n, void* destination, size_t cap)
{
    const u8* src = (const u8*)source;
    u8* dst = (u8*)destination;
    size_t op = 0;
    size_t anchor = 0;
    if (n >= MF_LIMIT + 1)
    {
        memset(table, 0, sizeof table);
        size_t limit = n - MF_LIMIT;
        size_t match_limit = n - LAST_LITERALS;
        size_t ip = 0;
        while (ip < limit)
        {
            unsigned int seq = read32(src + ip);
            unsigned int h = hash(seq);
            size_t ref = table[h];
            table[h] = (unsigned int)ip + 1u;
            if (ref == 0 || ip - (ref - 1) > 65535 || read32(src + ref - 1) != seq)
            {
                ip++;
                continue;
            }
            ref--;
            size_t length = MIN_MATCH;
            while (ip + length < match_limit && src[ref + length] == src[ip + length])
                length++;
            if (put_sequence(src, anchor, ip, ip - ref, length, dst, &op, cap) != 0)
                return fail(ENOSPC);
            ip += length;
            anchor = ip;
            if (ip - 2 < limit)
                table[hash(read32(src + ip - 2))] = (unsigned int)(ip - 2) + 1u;   // a later match is likelier
        }
    }
    if (put_sequence(src, anchor, n, 0, 0, dst, &op, cap) != 0)
        return fail(ENOSPC);
    return (int)op;
}
