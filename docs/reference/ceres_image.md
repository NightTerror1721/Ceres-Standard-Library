# `<ceres/image.h>`

Images from files and packs into surfaces (ceres/gfx.h), so sprites and tiles need not be compiled in as arrays. Two formats, told apart by their first bytes:

```c
QOI  the "Quite OK Image" format: lossless, about as small as PNG and decoded in one pass - the one to use.
BMP  Windows bitmaps as paint programs save them: 1, 4 and 8 bits through a palette, 16, 24 and 32 bits
     (plain or with bit fields), bottom-up or top-down. Compressed ones (RLE, JPEG, PNG inside) are refused.
```

```c
struct gfx_surface* hero = image_load("host:art/hero.qoi", RGB(255, 0, 255));
gfx_blit_key(hero, 0, 0, hero->w, hero->h, x, y, RGB(255, 0, 255));
image_free(hero);
```

A surface is 0x00RRGGBB and has no alpha: a pixel whose alpha is below 128 becomes `transparent` - the colour a sprite then keys out - and every other pixel keeps its colour. IMAGE_OPAQUE keeps the colour of every pixel.

Failures return NULL (or -1) with errno: EINVAL for data that is not an image this reads (or is cut short), ENOMEM when the pixels do not fit, and whatever fopen said for image_load. The decoders never read outside the data they are given. An image is at most IMAGE_MAX_SIDE pixels a side.

```c
#define IMAGE_OPAQUE     0xFFFFFFFFu
#define IMAGE_MAX_SIDE   8192

enum image_format { IMAGE_UNKNOWN = 0, IMAGE_QOI = 1, IMAGE_BMP = 2 };

// What the data is and its size, from its header alone.
enum image_format image_info(const void* data, size_t size, int* w, int* h);

// A new surface (one block: free it with image_free) holding the image.
struct gfx_surface* image_decode(const void* data, size_t size, unsigned int transparent);
// Into a surface of the image's own size: 0, or -1 (EINVAL also when the sizes differ).
int  image_decode_into(const void* data, size_t size, struct gfx_surface* dst, unsigned int transparent);
struct gfx_surface* image_load(const char* path, unsigned int transparent);   // any path fopen takes
void image_free(struct gfx_surface* s);

// A surface as a file: a malloc'd QOI (3 channels) or BMP (24 bits, bottom-up), its size in *size; NULL (ENOMEM).
void* image_encode_qoi(const struct gfx_surface* s, size_t* size);
void* image_encode_bmp(const struct gfx_surface* s, size_t* size);
int   image_save(const struct gfx_surface* s, const char* path);   // .bmp is BMP, anything else QOI; 0 or -1
```
