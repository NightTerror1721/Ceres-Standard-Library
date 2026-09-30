# `<ceres/endian.h>`

Little- and big-endian loads and stores over a byte buffer, so the on-disk formats (CeresFS, resource packs, saved games, LZ4 frames, QOI and BMP) read the same whatever the host and no file hand-rolls the shifts. The machine is little-endian, so the little ones compile to plain byte moves; `p` needs no alignment.

```c
static inline unsigned int le16(const unsigned char* p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static inline unsigned int le32(const unsigned char* p)
{
    return le16(p) | (le16(p + 2) << 16);
}

static inline void put_le16(unsigned char* p, unsigned int v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
}

static inline void put_le32(unsigned char* p, unsigned int v)
{
    put_le16(p, v);
    put_le16(p + 2, v >> 16);
}

static inline unsigned int be16(const unsigned char* p)
{
    return ((unsigned int)p[0] << 8) | (unsigned int)p[1];
}

static inline unsigned int be32(const unsigned char* p)
{
    return (be16(p) << 16) | be16(p + 2);
}
```
