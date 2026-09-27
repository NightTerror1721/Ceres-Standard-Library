// The GPU's text plane (ceres/text.h): drawing (clipped), and the frames it presents, which are in test_text.screen.
#include "ceres/test.h"
#include "ceres/text.h"

int main(void)
{
    TEST_SECTION("size");
    CHECK_EQ(text_cols(), 0);                               // nothing before text_init
    CHECK_EQ(text_rows(), 0);
    CHECK_EQ(text_get(0, 0), ' ');
    text_put(0, 0, 'x');                                    // drawing before init is harmless
    text_present();
    CHECK_EQ(text_init(0, 5), -1);
    CHECK_EQ(text_init(5, 0), -1);
    CHECK_EQ(text_init(201, 5), -1);
    CHECK_EQ(text_init(10, 101), -1);
    CHECK_EQ(text_init(-3, 4), -1);
    CHECK_EQ(text_init(20, 6), 0);
    CHECK_EQ(text_cols(), 20);
    CHECK_EQ(text_rows(), 6);
    CHECK_EQ(text_init(300, 6), -1);                        // a refused size changes nothing
    CHECK_EQ(text_cols(), 20);

    TEST_SECTION("points and clipping");
    text_put(3, 2, 'A');
    CHECK_EQ(text_get(3, 2), 'A');
    CHECK_EQ(text_get(4, 2), ' ');
    text_put(-1, 0, 'X');                                   // outside: not drawn, no harm
    text_put(20, 0, 'X');
    text_put(0, 6, 'X');
    text_put(0, -1, 'X');
    CHECK_EQ(text_get(-1, 0), ' ');
    CHECK_EQ(text_get(20, 0), ' ');
    CHECK_EQ(text_get(0, 6), ' ');
    text_put(19, 5, 'Z');                                   // the far corner is inside
    CHECK_EQ(text_get(19, 5), 'Z');
    text_clear('.');
    CHECK_EQ(text_get(0, 0), '.');
    CHECK_EQ(text_get(19, 5), '.');
    text_clear(' ');
    CHECK_EQ(text_get(3, 2), ' ');

    TEST_SECTION("a frame");
    text_box(0, 0, 20, 6);
    text_text(2, 1, "Ceres");
    text_printf(2, 2, "x=%d y=%d", 42, -7);
    text_hline(2, 3, 5, '=');
    text_vline(10, 1, 4, ':');
    text_text(12, 1, "ab\ncd\nef");                         // a newline goes to the next row, back at column 12
    text_present();

    TEST_SECTION("shapes");
    text_clear('.');
    text_fill(1, 1, 5, 3, '#');
    text_rect(8, 0, 6, 4, 'o');
    text_box(14, 2, 6, 4);
    text_rect(0, 5, 0, 3, '?');                             // an empty rectangle draws nothing
    text_box(2, 4, 1, 3);                                   // a box needs at least 2 x 2
    text_fill(-2, -2, 4, 4, '@');                           // partly outside: clipped
    text_hline(17, 0, 9, '~');                              // runs off the right edge
    text_vline(19, 3, 9, '!');                              // runs off the bottom
    text_present();

    TEST_SECTION("copy");
    text_clear(' ');
    text_text(0, 0, "abcdef");
    text_text(0, 1, "ghijkl");
    text_copy(3, 0, 0, 0, 6, 2);                            // to the right, overlapping itself
    CHECK_EQ(text_get(3, 0), 'a');
    CHECK_EQ(text_get(8, 0), 'f');
    CHECK_EQ(text_get(3, 1), 'g');
    CHECK_EQ(text_get(0, 0), 'a');                          // the part that was not covered is unchanged
    text_copy(0, 0, 3, 0, 6, 2);                            // and back to the left
    CHECK_EQ(text_get(0, 0), 'a');
    CHECK_EQ(text_get(5, 0), 'f');
    text_copy(0, 3, 0, 0, 6, 2);                            // downwards
    CHECK_EQ(text_get(0, 3), 'a');
    CHECK_EQ(text_get(5, 4), 'l');
    text_copy(0, 1, 0, 0, 6, 4);                            // downwards, overlapping
    CHECK_EQ(text_get(0, 1), 'a');
    CHECK_EQ(text_get(0, 2), 'g');
    text_copy(-3, 0, 0, 0, 6, 1);                           // partly off the grid: clipped, no harm
    text_present();

    TEST_SECTION("scroll");
    text_clear(' ');
    for (int y = 0; y < 6; y++)
        text_printf(0, y, "row %d", y);
    text_scroll(2);                                         // up two rows
    CHECK_EQ(text_get(4, 0), '2');
    CHECK_EQ(text_get(4, 3), '5');
    CHECK_EQ(text_get(0, 4), ' ');                          // the new rows are blank
    text_present();
    text_scroll(-1);                                        // down one row
    CHECK_EQ(text_get(4, 1), '2');
    CHECK_EQ(text_get(0, 0), ' ');
    text_scroll(0);
    CHECK_EQ(text_get(4, 1), '2');
    text_scroll(99);                                        // more than the grid: all blank
    CHECK_EQ(text_get(4, 1), ' ');
    text_scroll(-99);

    TEST_SECTION("a wide grid");
    CHECK_EQ(text_init(36, 3), 0);
    text_clear(' ');
    text_box(0, 0, 36, 3);
    text_text(2, 1, "012345678901234567890123456789012");
    text_present();
    text_shutdown();
    CHECK_EQ(text_cols(), 0);
    text_present();                                         // after shutdown it does nothing
    return test_summary();
}
