// Colour in the text grid: an attribute per cell, set for the drawing calls that follow it, carried by copy and
// scroll, taken by a clear, and shown as escape sequences only when something is coloured. The frames appear in
// the expected output, escapes and all.
#include "ceres/test.h"
#include "ceres/textfb.h"

int main(void)
{
    TEST_SECTION("attributes");
    CHECK_EQ(fb_init(6, 3), 0);
    CHECK_EQ((int)fb_attr(), 0);
    fb_set_attr(FB_ATTR(FB_RED, FB_BLUE));                // red on blue
    CHECK_EQ((int)fb_attr(), 0x41);
    fb_put(0, 0, 'A');
    CHECK_EQ((int)fb_get_attr(0, 0), 0x41);
    CHECK_EQ((int)fb_get_attr(1, 0), 0);                  // nothing was written there
    fb_put_attr(1, 0, 'B', FB_ATTR(FB_BRIGHT + FB_YELLOW, FB_BLACK));
    CHECK_EQ((int)fb_get_attr(1, 0), 0x0B);
    CHECK_EQ((int)fb_attr(), 0x41);                       // fb_put_attr leaves the current one alone
    fb_set_attr(0);
    fb_text(2, 0, "cd");                                  // plain again
    CHECK_EQ((int)fb_get_attr(2, 0), 0);
    CHECK_EQ((int)fb_get_attr(-1, 0), 0);                 // outside the grid
    CHECK_EQ((int)fb_get_attr(0, 9), 0);
    fb_present();

    TEST_SECTION("copy and scroll carry the colours");
    fb_copy(0, 1, 0, 0, 4, 1);
    CHECK_EQ((int)fb_get_attr(0, 1), 0x41);
    CHECK_EQ((int)fb_get_attr(1, 1), 0x0B);
    CHECK_EQ((int)fb_get_attr(2, 1), 0);
    fb_present();
    fb_scroll(1);                                         // the copy moves up to row 0
    CHECK_EQ((int)fb_get_attr(0, 0), 0x41);
    CHECK_EQ((int)fb_get_attr(0, 1), 0);                  // the row that came in is blank, in the current attribute
    fb_present();

    TEST_SECTION("a clear takes the current attribute");
    fb_set_attr(FB_ATTR(FB_WHITE, FB_GREEN));
    fb_clear('.');
    CHECK_EQ((int)fb_get_attr(5, 2), 0x27);
    fb_present();

    TEST_SECTION("a frame with no colour is plain text");
    fb_set_attr(0);
    fb_clear(' ');
    fb_text(0, 1, "plain");
    fb_present();
    fb_shutdown();
    return test_summary();
}
