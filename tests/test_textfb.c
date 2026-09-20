// The text framebuffer: drawing (clipped), and the frames it presents, which appear in the expected output.
#include "ceres/test.h"
#include "ceres/textfb.h"

int main(void)
{
    TEST_SECTION("size");
    CHECK_EQ(fb_cols(), 0);                               // nothing before fb_init
    CHECK_EQ(fb_rows(), 0);
    CHECK_EQ(fb_get(0, 0), ' ');
    fb_put(0, 0, 'x');                                    // drawing before init is harmless
    fb_present();
    CHECK_EQ(fb_init(0, 5), -1);
    CHECK_EQ(fb_init(5, 0), -1);
    CHECK_EQ(fb_init(201, 5), -1);
    CHECK_EQ(fb_init(10, 101), -1);
    CHECK_EQ(fb_init(-3, 4), -1);
    CHECK_EQ(fb_init(20, 6), 0);
    CHECK_EQ(fb_cols(), 20);
    CHECK_EQ(fb_rows(), 6);
    CHECK_EQ(fb_init(300, 6), -1);                        // a refused size changes nothing
    CHECK_EQ(fb_cols(), 20);

    TEST_SECTION("points and clipping");
    fb_put(3, 2, 'A');
    CHECK_EQ(fb_get(3, 2), 'A');
    CHECK_EQ(fb_get(4, 2), ' ');
    fb_put(-1, 0, 'X');                                   // outside: not drawn, no harm
    fb_put(20, 0, 'X');
    fb_put(0, 6, 'X');
    fb_put(0, -1, 'X');
    CHECK_EQ(fb_get(-1, 0), ' ');
    CHECK_EQ(fb_get(20, 0), ' ');
    CHECK_EQ(fb_get(0, 6), ' ');
    fb_put(19, 5, 'Z');                                   // the far corner is inside
    CHECK_EQ(fb_get(19, 5), 'Z');
    fb_clear('.');
    CHECK_EQ(fb_get(0, 0), '.');
    CHECK_EQ(fb_get(19, 5), '.');
    fb_clear(' ');
    CHECK_EQ(fb_get(3, 2), ' ');

    TEST_SECTION("a frame");
    fb_box(0, 0, 20, 6);
    fb_text(2, 1, "Ceres");
    fb_printf(2, 2, "x=%d y=%d", 42, -7);
    fb_hline(2, 3, 5, '=');
    fb_vline(10, 1, 4, ':');
    fb_text(12, 1, "ab\ncd\nef");                         // a newline goes to the next row, back at column 12
    fb_present();

    TEST_SECTION("shapes");
    fb_clear('.');
    fb_fill(1, 1, 5, 3, '#');
    fb_rect(8, 0, 6, 4, 'o');
    fb_box(14, 2, 6, 4);
    fb_rect(0, 5, 0, 3, '?');                             // an empty rectangle draws nothing
    fb_box(2, 4, 1, 3);                                   // a box needs at least 2 x 2
    fb_fill(-2, -2, 4, 4, '@');                           // partly outside: clipped
    fb_hline(17, 0, 9, '~');                              // runs off the right edge
    fb_vline(19, 3, 9, '!');                              // runs off the bottom
    fb_present();

    TEST_SECTION("copy");
    fb_clear(' ');
    fb_text(0, 0, "abcdef");
    fb_text(0, 1, "ghijkl");
    fb_copy(3, 0, 0, 0, 6, 2);                            // to the right, overlapping itself
    CHECK_EQ(fb_get(3, 0), 'a');
    CHECK_EQ(fb_get(8, 0), 'f');
    CHECK_EQ(fb_get(3, 1), 'g');
    CHECK_EQ(fb_get(0, 0), 'a');                          // the part that was not covered is unchanged
    fb_copy(0, 0, 3, 0, 6, 2);                            // and back to the left
    CHECK_EQ(fb_get(0, 0), 'a');
    CHECK_EQ(fb_get(5, 0), 'f');
    fb_copy(0, 3, 0, 0, 6, 2);                            // downwards
    CHECK_EQ(fb_get(0, 3), 'a');
    CHECK_EQ(fb_get(5, 4), 'l');
    fb_copy(0, 1, 0, 0, 6, 4);                            // downwards, overlapping
    CHECK_EQ(fb_get(0, 1), 'a');
    CHECK_EQ(fb_get(0, 2), 'g');
    fb_copy(-3, 0, 0, 0, 6, 1);                           // partly off the grid: clipped, no harm
    fb_present();

    TEST_SECTION("scroll");
    fb_clear(' ');
    for (int y = 0; y < 6; y++)
        fb_printf(0, y, "row %d", y);
    fb_scroll(2);                                         // up two rows
    CHECK_EQ(fb_get(4, 0), '2');
    CHECK_EQ(fb_get(4, 3), '5');
    CHECK_EQ(fb_get(0, 4), ' ');                          // the new rows are blank
    fb_present();
    fb_scroll(-1);                                        // down one row
    CHECK_EQ(fb_get(4, 1), '2');
    CHECK_EQ(fb_get(0, 0), ' ');
    fb_scroll(0);
    CHECK_EQ(fb_get(4, 1), '2');
    fb_scroll(99);                                        // more than the grid: all blank
    CHECK_EQ(fb_get(4, 1), ' ');
    fb_scroll(-99);

    TEST_SECTION("a wide grid");
    CHECK_EQ(fb_init(36, 3), 0);
    fb_clear(' ');
    fb_box(0, 0, 36, 3);
    fb_text(2, 1, "012345678901234567890123456789012");
    fb_present();
    fb_shutdown();
    CHECK_EQ(fb_cols(), 0);
    fb_present();                                         // after shutdown it does nothing
    return test_summary();
}
