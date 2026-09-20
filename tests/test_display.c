// The pixel display. A headless run presents to nothing, so this checks the parts a program can observe:
// the size handling, that drawing calls are accepted, and the colour packing.
#include "ceres/test.h"
#include "ceres/display.h"

static unsigned int frame[16 * 16];

int main(void)
{
    TEST_SECTION("size");
    CHECK_EQ(display_width(), 320);                       // the device starts at 320 x 200
    CHECK_EQ(display_height(), 200);
    CHECK_EQ(display_size(64, 48), 0);
    CHECK_EQ(display_width(), 64);
    CHECK_EQ(display_height(), 48);
    CHECK_EQ(display_size(0, 10), -1);
    CHECK_EQ(display_size(10, 0), -1);
    CHECK_EQ(display_size(-5, 10), -1);
    CHECK_EQ(display_size(1281, 10), -1);
    CHECK_EQ(display_size(10, 721), -1);
    CHECK_EQ(display_width(), 64);                        // refused sizes changed nothing
    CHECK_EQ(display_height(), 48);
    CHECK_EQ(display_size(1280, 720), 0);                 // the largest
    CHECK_EQ(display_width() * display_height(), 921600);
    CHECK_EQ(display_size(16, 16), 0);

    TEST_SECTION("colours");
    CHECK_EQ((int)RGB(1, 2, 3), 0x010203);
    CHECK_EQ((int)RGB(255, 255, 255), 0xFFFFFF);
    CHECK_EQ((int)RGB(255, 0, 0), 0xFF0000);
    CHECK_EQ((int)RGB(0, 0, 0), 0);
    CHECK_EQ((int)RGB(0x12, 0x34, 0x56), 0x123456);

    TEST_SECTION("drawing without a window");
    for (int i = 0; i < 16 * 16; i++)
        frame[i] = RGB(i & 255, 255 - (i & 255), i >> 2);
    display_blit(frame, 16, 16);                          // sends a whole frame
    display_show();                                       // presenting goes nowhere, and that is fine
    display_clear();
    display_put(RGB(255, 0, 0));
    display_put(RGB(0, 255, 0));
    display_show();
    CHECK_EQ(display_width(), 16);                        // none of that changed the size
    display_blit(0, 16, 16);                              // no pixels, no harm
    display_blit(frame, 0, 16);
    display_blit(frame, 16, -1);
    CHECK_EQ(display_width(), 16);

    TEST_SECTION("a blit of another size resizes the surface");
    display_blit(frame, 8, 8);                            // the first 64 pixels of the buffer
    CHECK_EQ(display_width(), 8);
    CHECK_EQ(display_height(), 8);
    display_blit(frame, 2000, 10);                        // a size the device refuses: nothing is drawn, nothing changes
    CHECK_EQ(display_width(), 8);
    return test_summary();
}
