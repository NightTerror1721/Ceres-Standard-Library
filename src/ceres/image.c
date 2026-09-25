// QOI and BMP images. See ceres/image.h.
#include "ceres/image.h"
#include "stdlib.h"
#include "string.h"
#include "stdio.h"
#include "errno.h"

typedef unsigned char u8;

static unsigned int le16(const u8* p) { return (unsigned int)p[0] | ((unsigned int)p[1] << 8); }
static unsigned int le32(const u8* p) { return le16(p) | (le16(p + 2) << 16); }
static unsigned int be32(const u8* p)
{
    return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) | ((unsigned int)p[2] << 8) | (unsigned int)p[3];
}

static unsigned int pixel(unsigned int r, unsigned int g, unsigned int b, unsigned int a, unsigned int transparent)
{
    if (a < 128u && transparent != IMAGE_OPAQUE)
        return transparent;
    return ((r & 255u) << 16) | ((g & 255u) << 8) | (b & 255u);
}

static int sides_ok(long long w, long long h)
{
    return w > 0 && h > 0 && w <= IMAGE_MAX_SIDE && h <= IMAGE_MAX_SIDE;
}

// ---- QOI ----

#define QOI_HEADER 14

static int qoi_header(const u8* p, size_t n, int* w, int* h)
{
    if (n < QOI_HEADER + 8 || memcmp(p, "qoif", 4) != 0)
        return -1;
    unsigned int width = be32(p + 4), height = be32(p + 8);
    if (!sides_ok(width, height) || (p[12] != 3 && p[12] != 4) || p[13] > 1)
        return -1;
    *w = (int)width;
    *h = (int)height;
    return 0;
}

static int qoi_hash(unsigned int r, unsigned int g, unsigned int b, unsigned int a)
{
    return (int)((r * 3u + g * 5u + b * 7u + a * 11u) % 64u);
}

static int qoi_decode(const u8* p, size_t n, unsigned int* px, int w, int h, unsigned int transparent)
{
    u8 index[64][4];
    memset(index, 0, sizeof index);
    unsigned int r = 0, g = 0, b = 0, a = 255;
    size_t pos = QOI_HEADER;
    size_t total = (size_t)w * (size_t)h;
    unsigned int run = 0;
    for (size_t i = 0; i < total; i++)
    {
        if (run > 0)
            run--;
        else
        {
            if (pos >= n)
                return -1;
            unsigned int op = p[pos++];
            if (op == 0xFEu || op == 0xFFu)
            {
                size_t need = op == 0xFEu ? 3 : 4;
                if (n - pos < need)
                    return -1;
                r = p[pos];
                g = p[pos + 1];
                b = p[pos + 2];
                if (op == 0xFFu)
                    a = p[pos + 3];
                pos += need;
            }
            else if ((op >> 6) == 0)                                // an earlier colour, from the index
            {
                r = index[op][0];
                g = index[op][1];
                b = index[op][2];
                a = index[op][3];
            }
            else if ((op >> 6) == 1)                                // a small difference
            {
                r = (r + ((op >> 4) & 3u) - 2u) & 255u;
                g = (g + ((op >> 2) & 3u) - 2u) & 255u;
                b = (b + (op & 3u) - 2u) & 255u;
            }
            else if ((op >> 6) == 2)                                // a difference by green
            {
                if (pos >= n)
                    return -1;
                unsigned int second = p[pos++];
                unsigned int dg = (op & 63u) - 32u;
                r = (r + dg - 8u + ((second >> 4) & 15u)) & 255u;
                g = (g + dg) & 255u;
                b = (b + dg - 8u + (second & 15u)) & 255u;
            }
            else
                run = op & 63u;                                     // this pixel and `run` more like the last
            int slot = qoi_hash(r, g, b, a);
            index[slot][0] = (u8)r;
            index[slot][1] = (u8)g;
            index[slot][2] = (u8)b;
            index[slot][3] = (u8)a;
        }
        px[i] = pixel(r, g, b, a, transparent);
    }
    return 0;
}

void* image_encode_qoi(const struct gfx_surface* s, size_t* size)
{
    size_t total = (size_t)s->w * (size_t)s->h;
    u8* out = (u8*)malloc(QOI_HEADER + total * 4 + 8);
    if (out == 0)
    {
        errno = ENOMEM;
        return 0;
    }
    memcpy(out, "qoif", 4);
    unsigned int dims[2] = { (unsigned int)s->w, (unsigned int)s->h };
    for (int k = 0; k < 2; k++)
        for (int j = 0; j < 4; j++)
            out[4 + k * 4 + j] = (u8)(dims[k] >> (24 - 8 * j));
    out[12] = 3;                                                     // RGB
    out[13] = 0;                                                     // sRGB
    size_t o = QOI_HEADER;
    unsigned int index[64];
    int used[64];
    memset(used, 0, sizeof used);
    unsigned int previous = 0;                                       // black, alpha 255
    unsigned int run = 0;
    for (size_t i = 0; i < total; i++)
    {
        unsigned int c = s->px[i] & 0xFFFFFFu;
        if (c == previous)
        {
            run++;
            if (run == 62 || i == total - 1)
            {
                out[o++] = (u8)(0xC0u | (run - 1));
                run = 0;
            }
            continue;
        }
        if (run > 0)
        {
            out[o++] = (u8)(0xC0u | (run - 1));
            run = 0;
        }
        unsigned int r = c >> 16, g = (c >> 8) & 255u, b = c & 255u;
        int slot = qoi_hash(r, g, b, 255);
        if (used[slot] && index[slot] == c)
            out[o++] = (u8)slot;
        else
        {
            index[slot] = c;
            used[slot] = 1;
            int vr = (int)(signed char)(u8)(r - (previous >> 16));
            int vg = (int)(signed char)(u8)(g - ((previous >> 8) & 255u));
            int vb = (int)(signed char)(u8)(b - (previous & 255u));
            int vgr = vr - vg, vgb = vb - vg;
            if (vr >= -2 && vr <= 1 && vg >= -2 && vg <= 1 && vb >= -2 && vb <= 1)
                out[o++] = (u8)(0x40 | ((vr + 2) << 4) | ((vg + 2) << 2) | (vb + 2));
            else if (vg >= -32 && vg <= 31 && vgr >= -8 && vgr <= 7 && vgb >= -8 && vgb <= 7)
            {
                out[o++] = (u8)(0x80 | (vg + 32));
                out[o++] = (u8)(((vgr + 8) << 4) | (vgb + 8));
            }
            else
            {
                out[o++] = 0xFE;
                out[o++] = (u8)r;
                out[o++] = (u8)g;
                out[o++] = (u8)b;
            }
        }
        previous = c;
    }
    static const u8 end[8] = { 0, 0, 0, 0, 0, 0, 0, 1 };
    memcpy(out + o, end, 8);
    *size = o + 8;
    return out;
}

// ---- BMP ----

struct bmp
{
    int w, h, top_down, bpp;
    size_t pixels;              // where the rows start
    size_t palette;             // where the palette starts, and its entries and their size
    unsigned int colors, entry;
    unsigned int mask[4];       // red, green, blue, alpha
};

static int bmp_header(const u8* p, size_t n, struct bmp* b)
{
    if (n < 26 || p[0] != 'B' || p[1] != 'M')
        return -1;
    unsigned int dib = le32(p + 14);
    unsigned int compression = 0;
    long long w, h;
    b->pixels = le32(p + 10);
    if (dib == 12)
    {
        w = le16(p + 18);
        h = (short)le16(p + 20);
        b->bpp = (int)le16(p + 24);
        b->colors = 0;
        b->entry = 3;
    }
    else if (dib >= 40 && n >= 54)
    {
        w = (int)le32(p + 18);
        h = (int)le32(p + 22);
        b->bpp = (int)le16(p + 28);
        compression = le32(p + 30);
        b->colors = le32(p + 46);
        b->entry = 4;
    }
    else
        return -1;
    b->top_down = h < 0;
    if (h < 0)
        h = -h;
    if (!sides_ok(w, h))
        return -1;
    b->w = (int)w;
    b->h = (int)h;
    b->palette = 14 + (size_t)dib;
    memset(b->mask, 0, sizeof b->mask);
    switch (b->bpp)
    {
    case 1: case 4: case 8:
        if (compression != 0)
            return -1;                                              // RLE
        if (b->colors == 0 || b->colors > (1u << b->bpp))
            b->colors = 1u << b->bpp;
        if (b->palette > n || (n - b->palette) / b->entry < b->colors)
            return -1;
        break;
    case 24:
        if (compression != 0)
            return -1;
        break;
    case 16: case 32:
        if (compression == 0)
        {
            b->mask[0] = b->bpp == 16 ? 0x7C00u : 0xFF0000u;
            b->mask[1] = b->bpp == 16 ? 0x03E0u : 0x00FF00u;
            b->mask[2] = b->bpp == 16 ? 0x001Fu : 0x0000FFu;
        }
        else if (compression == 3 || compression == 6)              // bit fields
        {
            size_t at = dib >= 52 ? 54 : b->palette;                // in the header, or right after it
            unsigned int count = compression == 6 || dib >= 56 ? 4 : 3;
            if (at + count * 4 > n)
                return -1;
            for (unsigned int k = 0; k < count; k++)
                b->mask[k] = le32(p + at + k * 4);
        }
        else
            return -1;
        break;
    default:
        return -1;
    }
    size_t stride = (((size_t)b->w * (size_t)b->bpp + 31) / 32) * 4;
    if (b->pixels > n || (n - b->pixels) / stride < (size_t)b->h)
        return -1;
    return 0;
}

// The field `mask` of v, as 0..255.
static unsigned int field(unsigned int v, unsigned int mask)
{
    if (mask == 0)
        return 0;
    int shift = 0;
    while (((mask >> shift) & 1u) == 0)
        shift++;
    unsigned int top = mask >> shift;
    int bits = 0;
    while ((top >> bits) & 1u)
        bits++;
    unsigned int value = (v & mask) >> shift;
    if (bits >= 8)
        return value >> (bits - 8);
    return value * 255u / ((1u << bits) - 1u);
}

static int bmp_decode(const u8* p, const struct bmp* b, unsigned int* px, unsigned int transparent)
{
    size_t stride = (((size_t)b->w * (size_t)b->bpp + 31) / 32) * 4;
    for (int y = 0; y < b->h; y++)
    {
        const u8* row = p + b->pixels + stride * (size_t)(b->top_down ? y : b->h - 1 - y);
        unsigned int* out = px + (size_t)y * (size_t)b->w;
        for (int x = 0; x < b->w; x++)
        {
            unsigned int r, g, bl, a = 255;
            if (b->bpp <= 8)
            {
                unsigned int per = 8u / (unsigned int)b->bpp;
                unsigned int shift = 8u - (unsigned int)b->bpp * (1u + (unsigned int)x % per);
                unsigned int i = (row[(unsigned int)x / per] >> shift) & ((1u << b->bpp) - 1u);
                if (i >= b->colors)
                    i = 0;
                const u8* e = p + b->palette + i * b->entry;
                bl = e[0];
                g = e[1];
                r = e[2];
            }
            else if (b->bpp == 24)
            {
                const u8* e = row + x * 3;
                bl = e[0];
                g = e[1];
                r = e[2];
            }
            else
            {
                unsigned int v = b->bpp == 16 ? le16(row + x * 2) : le32(row + x * 4);
                r = field(v, b->mask[0]);
                g = field(v, b->mask[1]);
                bl = field(v, b->mask[2]);
                if (b->mask[3] != 0)
                    a = field(v, b->mask[3]);
            }
            out[x] = pixel(r, g, bl, a, transparent);
        }
    }
    return 0;
}

void* image_encode_bmp(const struct gfx_surface* s, size_t* size)
{
    size_t stride = ((size_t)s->w * 3 + 3) & ~(size_t)3;
    size_t total = 54 + stride * (size_t)s->h;
    u8* out = (u8*)calloc(total, 1);
    if (out == 0)
    {
        errno = ENOMEM;
        return 0;
    }
    unsigned int fields[][2] = {
        { 2, (unsigned int)total }, { 10, 54 }, { 14, 40 }, { 18, (unsigned int)s->w }, { 22, (unsigned int)s->h },
        { 34, (unsigned int)(stride * (size_t)s->h) }, { 38, 2835 }, { 42, 2835 } };   // 72 dots an inch
    out[0] = 'B';
    out[1] = 'M';
    for (int k = 0; k < 8; k++)
        for (int j = 0; j < 4; j++)
            out[fields[k][0] + (unsigned int)j] = (u8)(fields[k][1] >> (8 * j));
    out[26] = 1;                                                     // planes
    out[28] = 24;                                                    // bits a pixel
    for (int y = 0; y < s->h; y++)
    {
        u8* row = out + 54 + stride * (size_t)(s->h - 1 - y);       // bottom-up
        const unsigned int* in = s->px + (size_t)y * (size_t)s->w;
        for (int x = 0; x < s->w; x++)
        {
            row[x * 3] = (u8)in[x];
            row[x * 3 + 1] = (u8)(in[x] >> 8);
            row[x * 3 + 2] = (u8)(in[x] >> 16);
        }
    }
    *size = total;
    return out;
}

// ---- either ----

enum image_format image_info(const void* data, size_t size, int* w, int* h)
{
    const u8* p = (const u8*)data;
    int iw = 0, ih = 0;
    enum image_format format = IMAGE_UNKNOWN;
    struct bmp b;
    if (p != 0 && qoi_header(p, size, &iw, &ih) == 0)
        format = IMAGE_QOI;
    else if (p != 0 && bmp_header(p, size, &b) == 0)
    {
        format = IMAGE_BMP;
        iw = b.w;
        ih = b.h;
    }
    if (w != 0) *w = iw;
    if (h != 0) *h = ih;
    return format;
}

int image_decode_into(const void* data, size_t size, struct gfx_surface* dst, unsigned int transparent)
{
    const u8* p = (const u8*)data;
    int w, h;
    enum image_format format = image_info(data, size, &w, &h);
    if (format == IMAGE_UNKNOWN || dst == 0 || dst->w != w || dst->h != h)
    {
        errno = EINVAL;
        return -1;
    }
    int result;
    if (format == IMAGE_QOI)
        result = qoi_decode(p, size, dst->px, w, h, transparent);
    else
    {
        struct bmp b;
        bmp_header(p, size, &b);
        result = bmp_decode(p, &b, dst->px, transparent);
    }
    if (result != 0)
        errno = EINVAL;
    return result;
}

struct gfx_surface* image_decode(const void* data, size_t size, unsigned int transparent)
{
    int w, h;
    if (image_info(data, size, &w, &h) == IMAGE_UNKNOWN)
    {
        errno = EINVAL;
        return 0;
    }
    struct gfx_surface* s = (struct gfx_surface*)malloc(sizeof(struct gfx_surface) + (size_t)w * (size_t)h * 4u);
    if (s == 0)
    {
        errno = ENOMEM;
        return 0;
    }
    s->px = (unsigned int*)(s + 1);
    s->w = w;
    s->h = h;
    if (image_decode_into(data, size, s, transparent) != 0)
    {
        free(s);
        return 0;
    }
    return s;
}

void image_free(struct gfx_surface* s)
{
    free(s);
}

// A whole file into a malloc'd buffer: the stream need not know its size (the terminal does not).
static u8* read_all(const char* path, size_t* size)
{
    FILE* f = fopen(path, "rb");
    if (f == 0)
        return 0;
    size_t cap = 4096, n = 0;
    u8* buf = (u8*)malloc(cap);
    while (buf != 0)
    {
        if (n == cap)
        {
            u8* bigger = (u8*)realloc(buf, cap * 2);
            if (bigger == 0)
            {
                free(buf);
                buf = 0;
                break;
            }
            buf = bigger;
            cap *= 2;
        }
        size_t got = fread(buf + n, 1, cap - n, f);
        n += got;
        if (got == 0)
            break;
    }
    int bad = buf != 0 && ferror(f);
    fclose(f);
    if (buf == 0)
    {
        errno = ENOMEM;
        return 0;
    }
    if (bad)
    {
        free(buf);
        errno = EIO;
        return 0;
    }
    *size = n;
    return buf;
}

struct gfx_surface* image_load(const char* path, unsigned int transparent)
{
    size_t size;
    u8* data = read_all(path, &size);
    if (data == 0)
        return 0;
    struct gfx_surface* s = image_decode(data, size, transparent);
    int e = errno;
    free(data);
    errno = e;
    return s;
}

int image_save(const struct gfx_surface* s, const char* path)
{
    size_t length = strlen(path);
    int bmp = length >= 4 && (strcmp(path + length - 4, ".bmp") == 0 || strcmp(path + length - 4, ".BMP") == 0);
    size_t size;
    void* data = bmp ? image_encode_bmp(s, &size) : image_encode_qoi(s, &size);
    if (data == 0)
        return -1;
    FILE* f = fopen(path, "wb");
    int ok = f != 0 && fwrite(data, 1, size, f) == size;
    if (f != 0 && fclose(f) != 0)
        ok = 0;
    free(data);
    return ok ? 0 : -1;
}
