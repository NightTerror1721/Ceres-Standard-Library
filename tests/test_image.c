// Images (ceres/image.h): QOI built by hand op by op, BMPs of every depth built by hand and two saved by .NET's
// System.Drawing (tests/data/host/images, read through the host directory), surfaces that go out as QOI and BMP and
// come back the same, a file saved and loaded again, and broken data refused.
#include "ceres/test.h"
#include "ceres/image.h"
#include "ceres/color.h"
#include "string.h"
#include "stdlib.h"
#include "errno.h"

#define KEY RGB(255, 0, 255)

static unsigned char data[256];

static void put32(unsigned char* p, unsigned int v)
{
    for (int i = 0; i < 4; i++)
        p[i] = (unsigned char)(v >> (8 * i));
}

// The headers of a BMP with a 40-byte info header: `extra` bytes (palette, masks) come before the rows.
static void bmp_header(int w, int h, int bpp, unsigned int compression, unsigned int colors, int extra, int size)
{
    memset(data, 0, sizeof data);
    data[0] = 'B';
    data[1] = 'M';
    put32(data + 2, (unsigned int)size);
    put32(data + 10, 54u + (unsigned int)extra);
    put32(data + 14, 40);
    put32(data + 18, (unsigned int)w);
    put32(data + 22, (unsigned int)h);
    data[26] = 1;
    data[28] = (unsigned char)bpp;
    put32(data + 30, compression);
    put32(data + 46, colors);
}

static int same(const struct gfx_surface* a, const struct gfx_surface* b)
{
    return a->w == b->w && a->h == b->h && memcmp(a->px, b->px, (size_t)a->w * (size_t)a->h * 4u) == 0;
}

int main(void)
{
    TEST_SECTION("QOI by hand");
    static const unsigned char qoi[] = {
        'q', 'o', 'i', 'f', 0, 0, 0, 2, 0, 0, 0, 3, 4, 0,
        0xFE, 255, 0, 0,                  // red
        0x7A,                             // a small difference: red + 1 wraps to 0, so black
        0xFF, 10, 20, 30, 0,              // with alpha 0: transparent
        0x32,                             // index 50: red again
        0xBF, 0x88,                       // a difference by green: +31 in all three, from red
        0xC0,                             // a run of one
        0, 0, 0, 0, 0, 0, 0, 1 };
    int w = 0, h = 0;
    CHECK_EQ(image_info(qoi, sizeof qoi, &w, &h), IMAGE_QOI);
    CHECK(w == 2 && h == 3);
    struct gfx_surface* s = image_decode(qoi, sizeof qoi, KEY);
    CHECK(s != 0);
    CHECK_EQ(s->px[0], RGB(255, 0, 0));
    CHECK_EQ(s->px[1], RGB(0, 0, 0));
    CHECK_EQ(s->px[2], KEY);
    CHECK_EQ(s->px[3], RGB(255, 0, 0));
    CHECK_EQ(s->px[4], RGB(30, 31, 31));                         // 255 + 31 wraps to 30
    CHECK_EQ(s->px[5], RGB(30, 31, 31));
    image_free(s);
    s = image_decode(qoi, sizeof qoi, IMAGE_OPAQUE);
    CHECK_EQ(s->px[2], RGB(10, 20, 30));                         // its own colour
    image_free(s);
    errno = 0;
    CHECK(image_decode(qoi, 20, KEY) == 0);                      // cut short: not even a header
    CHECK_EQ(errno, EINVAL);
    CHECK(image_decode(qoi, 22, KEY) == 0);                      // a header, and the chunks cut short
    CHECK(image_decode(qoi, 27, KEY) == 0);
    static unsigned char huge[40];
    memcpy(huge, qoi, 14);
    huge[6] = 0x1F;                                              // 7938 x 3: far more pixels than 40 bytes can hold
    CHECK_EQ(image_info(huge, sizeof huge, &w, &h), IMAGE_UNKNOWN);
    unsigned char big[64];
    memcpy(big, qoi, sizeof qoi);
    big[4] = 0x7F;                                               // 2 billion pixels wide
    CHECK_EQ(image_info(big, sizeof big, &w, &h), IMAGE_UNKNOWN);

    TEST_SECTION("BMP by hand");
    bmp_header(3, 2, 1, 0, 0, 8, 70);                            // 1 bit: black and white, bottom-up
    put32(data + 54, 0x00000000);
    put32(data + 58, 0x00FFFFFF);
    data[62] = 0xA0;                                             // the bottom row: 1 0 1
    data[66] = 0x40;                                             // the top row: 0 1 0
    CHECK_EQ(image_info(data, 70, &w, &h), IMAGE_BMP);
    s = image_decode(data, 70, KEY);
    CHECK(s != 0 && s->w == 3 && s->h == 2);
    CHECK(s->px[0] == 0 && s->px[1] == 0xFFFFFFu && s->px[2] == 0);
    CHECK(s->px[3] == 0xFFFFFFu && s->px[4] == 0 && s->px[5] == 0xFFFFFFu);
    image_free(s);
    CHECK(image_decode(data, 66, KEY) == 0);                     // a row missing

    bmp_header(3, -1, 4, 0, 2, 8, 66);                           // 4 bits, two colours, top-down
    put32(data + 54, 0x00112233);
    put32(data + 58, 0x00445566);
    data[62] = 0x01;
    data[63] = 0x70;                                             // index 7 is past the palette: the first colour
    s = image_decode(data, 66, KEY);
    CHECK(s != 0);
    CHECK(s->px[0] == 0x112233u && s->px[1] == 0x445566u && s->px[2] == 0x112233u);
    image_free(s);

    bmp_header(2, 1, 16, 0, 0, 0, 58);                           // 16 bits, 5-5-5
    data[54] = 0x1F;                                             // pure blue
    data[55] = 0x00;
    data[56] = 0x00;
    data[57] = 0x7C;                                             // pure red
    s = image_decode(data, 58, KEY);
    CHECK(s != 0 && s->px[0] == 0x0000FFu && s->px[1] == 0xFF0000u);
    image_free(s);

    bmp_header(2, 1, 32, 6, 0, 16, 78);                          // 32 bits with fields, alpha too
    put32(data + 54, 0x0000FF00);                                // red in the second byte...
    put32(data + 58, 0x00FF0000);
    put32(data + 62, 0xFF000000);
    put32(data + 66, 0x000000FF);                                // ...and alpha in the first
    put32(data + 70, 0x332211FF);                                // opaque
    put32(data + 74, 0x66554400);                                // transparent
    s = image_decode(data, 78, KEY);
    CHECK(s != 0 && s->px[0] == RGB(0x11, 0x22, 0x33) && s->px[1] == KEY);
    image_free(s);

    bmp_header(1, 1, 32, 3, 0, 12, 70);                          // a mask of all ones: 32 bits, not a hang
    put32(data + 54, 0xFFFFFFFFu);
    put32(data + 58, 0x0000FF00u);
    put32(data + 62, 0x000000FFu);
    put32(data + 66, 0x12345678u);
    s = image_decode(data, 70, KEY);
    CHECK(s != 0 && s->px[0] == RGB(0x12, 0x56, 0x78));
    image_free(s);

    bmp_header(2, 1, 8, 1, 0, 0, 60);                            // RLE8: refused
    CHECK_EQ(image_info(data, 60, &w, &h), IMAGE_UNKNOWN);
    CHECK_EQ(image_info("GIF89a", 6, &w, &h), IMAGE_UNKNOWN);

    TEST_SECTION("BMPs saved by System.Drawing");
    s = image_load("host:images/dotnet24.bmp", KEY);
    CHECK(s != 0 && s->w == 5 && s->h == 3);
    int right = 1;
    for (int y = 0; y < 3; y++)
        for (int x = 0; x < 5; x++)
            right = right && s->px[y * 5 + x] == RGB(x * 50, y * 100, 7 + x + y);
    CHECK(right);
    image_free(s);
    s = image_load("host:images/dotnet32.bmp", KEY);             // 32 bits, no fields: the fourth byte is not alpha
    CHECK(s != 0 && s->w == 3 && s->h == 2);
    CHECK(s->px[0] == RGB(10, 20, 30) && s->px[1] == RGB(1, 2, 3) && s->px[2] == RGB(40, 50, 60));
    CHECK(s->px[3] == RGB(255, 255, 255) && s->px[4] == 0 && s->px[5] == RGB(90, 80, 70));
    image_free(s);
    errno = 0;
    CHECK(image_load("host:images/missing.qoi", KEY) == 0);
    CHECK_EQ(errno, ENOENT);

    TEST_SECTION("out and back");
    static unsigned int pixels[37 * 9];
    struct gfx_surface picture = { pixels, 37, 9 };
    for (int y = 0; y < 9; y++)
        for (int x = 0; x < 37; x++)
        {
            unsigned int c = RGB(x * 7, y * 28, (x * y) & 255);  // gradients: every kind of QOI step
            if (x > 20 && x < 30)
                c = RGB(1, 2, 3);                                // runs
            if (x == 33)
                c = RGB(255 - y, 0, y * 30);                     // big jumps
            pixels[y * 37 + x] = c;
        }
    size_t size = 0;
    void* encoded = image_encode_qoi(&picture, &size);
    CHECK(encoded != 0 && size > 22 && size < 37 * 9 * 4);
    s = image_decode(encoded, size, KEY);
    CHECK(s != 0 && same(s, &picture));
    image_free(s);
    free(encoded);
    encoded = image_encode_bmp(&picture, &size);
    CHECK_EQ((int)size, 54 + 112 * 9);                           // 37 * 3 = 111, padded to 112
    s = image_decode(encoded, size, KEY);
    CHECK(s != 0 && same(s, &picture));
    image_free(s);
    free(encoded);
    CHECK_EQ(image_save(&picture, "host:shot.qoi"), 0);
    CHECK_EQ(image_save(&picture, "host:shot.bmp"), 0);
    s = image_load("host:shot.qoi", KEY);
    CHECK(s != 0 && same(s, &picture));
    image_free(s);
    s = image_load("host:shot.bmp", KEY);
    CHECK(s != 0 && same(s, &picture));
    image_free(s);
    static unsigned int small[4];
    struct gfx_surface wrong = { small, 2, 2 };
    CHECK_EQ(image_decode_into(qoi, sizeof qoi, &wrong, KEY), -1);   // 2 x 3 does not go into 2 x 2
    return test_summary();
}
