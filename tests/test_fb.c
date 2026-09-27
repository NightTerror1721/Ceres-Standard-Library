// The GPU's bitmap plane (ceres/fb.h): its setup, the two buffers and the flip at the vertical blank, and the pixels,
// which a program reads back from VRAM like any memory.
#include "ceres/test.h"
#include "ceres/fb.h"

static unsigned int frame[16 * 16];

int main(void)
{
    TEST_SECTION("before fb_init");
    CHECK_EQ(fb_width(), 0);
    CHECK(fb_pixels() == 0);
    fb_present();                                         // nothing to show: no harm
    fb_clear(0);

    TEST_SECTION("init");
    CHECK_EQ(fb_init(0, 10), -1);
    CHECK_EQ(fb_init(10, 0), -1);
    CHECK_EQ(fb_init(-5, 10), -1);
    CHECK_EQ(fb_init(4000, 10), -1);                      // wider than the machine's largest screen
    CHECK_EQ(fb_init(64, 48), 0);
    CHECK_EQ(fb_width(), 64);
    CHECK_EQ(fb_height(), 48);
    CHECK_EQ(fb_indexed(), 0);
    CHECK_EQ(video_width(), 64);                          // the screen follows the picture
    CHECK_EQ(video_height(), 48);
    CHECK_EQ(video_level(), VIDEO_BITMAP);
    unsigned int* a = (unsigned int*)fb_pixels();
    CHECK(a != 0);
    CHECK((unsigned int)a >= VRAM_BASE);                  // in video memory
    CHECK_EQ(a[0], 0u);                                   // black to start with
    CHECK_EQ(a[64 * 48 - 1], 0u);

    TEST_SECTION("colours");
    CHECK_EQ((int)RGB(1, 2, 3), 0x010203);
    CHECK_EQ((int)RGB(255, 255, 255), 0xFFFFFF);
    CHECK_EQ((int)RGB(0x12, 0x34, 0x56), 0x123456);

    TEST_SECTION("the flip at the vertical blank");
    a[0] = RGB(255, 0, 0);
    unsigned int before = video_frame();
    fb_present();
    CHECK(video_frame() != before);                       // it waited for a blank
    unsigned int* b = (unsigned int*)fb_pixels();
    CHECK(b != a);                                        // now the other buffer is drawn into
    CHECK_EQ(b[0], 0u);
    fb_keep();                                            // ... with the frame just shown in it
    CHECK_EQ(b[0], RGB(255, 0, 0));
    fb_present();
    CHECK(fb_pixels() == a);                              // and back

    TEST_SECTION("clear and blit");
    fb_clear(RGB(1, 2, 3));
    CHECK_EQ(a[0], RGB(1, 2, 3));
    CHECK_EQ(a[64 * 48 - 1], RGB(1, 2, 3));
    CHECK_EQ(fb_init(16, 16), 0);
    for (int i = 0; i < 16 * 16; i++)
        frame[i] = RGB(i & 255, 255 - (i & 255), i >> 2);
    fb_blit(frame);
    CHECK_EQ(((unsigned int*)fb_pixels())[17], frame[17]);
    CHECK_EQ(((unsigned int*)fb_pixels())[255], frame[255]);
    fb_blit(0);                                           // no picture, no harm
    fb_present();

    TEST_SECTION("indexed");
    CHECK_EQ(fb_init_indexed(32, 16), 0);
    CHECK_EQ(fb_indexed(), 1);
    CHECK_EQ((int)mmio_r32(FB_FORMAT), FB_FORMAT_I8);
    CHECK_EQ((int)mmio_r32(FB_PITCH), 32);
    unsigned char* p8 = (unsigned char*)fb_pixels();
    fb_clear(5);
    CHECK_EQ(p8[0], 5);
    CHECK_EQ(p8[32 * 16 - 1], 5);
    fb_set_palette(3, RGB(9, 8, 7));
    const unsigned int* palette = (const unsigned int*)mmio_r32(FB_PALETTE_BASE);
    CHECK_EQ(palette[3], RGB(9, 8, 7));
    CHECK_EQ(palette[1], 0xAA0000u);                      // the rest starts as the text's (ANSI red)
    fb_scroll(-1, 20);                                    // 31, 4
    CHECK_EQ((int)mmio_r32(FB_SCROLL_X), 31);
    CHECK_EQ((int)mmio_r32(FB_SCROLL_Y), 4);
    fb_present();

    TEST_SECTION("shutdown");
    fb_shutdown();
    CHECK_EQ(fb_width(), 0);
    CHECK_EQ(video_level(), VIDEO_TEXT);
    CHECK_EQ((int)mmio_r32(FB_ENABLE), 0);
    return test_summary();
}
