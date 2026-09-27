// Colour in the text grid: an attribute per cell, set for the drawing calls that follow it, carried by copy and
// scroll, taken by a clear. What the cells hold on the screen is read back from VRAM.
#include "ceres/test.h"
#include "ceres/text.h"

int main(void)
{
    TEST_SECTION("attributes");
    CHECK_EQ(text_init(6, 3), 0);
    CHECK_EQ((int)text_attr(), 0);
    text_set_attr(TEXT_ATTR(TEXT_RED, TEXT_BLUE));                // red on blue
    CHECK_EQ((int)text_attr(), 0x41);
    text_put(0, 0, 'A');
    CHECK_EQ((int)text_get_attr(0, 0), 0x41);
    CHECK_EQ((int)text_get_attr(1, 0), 0);                  // nothing was written there
    text_put_attr(1, 0, 'B', TEXT_ATTR(TEXT_BRIGHT + TEXT_YELLOW, TEXT_BLACK));
    CHECK_EQ((int)text_get_attr(1, 0), 0x0B);
    CHECK_EQ((int)text_attr(), 0x41);                       // text_put_attr leaves the current one alone
    text_set_attr(0);
    text_text(2, 0, "cd");                                  // plain again
    CHECK_EQ((int)text_get_attr(2, 0), 0);
    CHECK_EQ((int)text_get_attr(-1, 0), 0);                 // outside the grid
    CHECK_EQ((int)text_get_attr(0, 9), 0);
    text_present();

    TEST_SECTION("copy and scroll carry the colours");
    text_copy(0, 1, 0, 0, 4, 1);
    CHECK_EQ((int)text_get_attr(0, 1), 0x41);
    CHECK_EQ((int)text_get_attr(1, 1), 0x0B);
    CHECK_EQ((int)text_get_attr(2, 1), 0);
    text_present();
    text_scroll(1);                                         // the copy moves up to row 0
    CHECK_EQ((int)text_get_attr(0, 0), 0x41);
    CHECK_EQ((int)text_get_attr(0, 1), 0);                  // the row that came in is blank, in the current attribute
    text_present();

    TEST_SECTION("a clear takes the current attribute");
    text_set_attr(TEXT_ATTR(TEXT_WHITE, TEXT_GREEN));
    text_clear('.');
    CHECK_EQ((int)text_get_attr(5, 2), 0x27);
    text_present();

    TEST_SECTION("a frame with no colour is plain text");
    text_set_attr(0);
    text_clear(' ');
    text_text(0, 1, "plain");
    text_present();

    TEST_SECTION("the screen's cells");
    text_set_attr(TEXT_ATTR(TEXT_RED, TEXT_BLUE));
    text_put(0, 0, 'A');
    text_set_attr(0);
    text_put(1, 0, 'b');
    text_present();
    const unsigned short* cells = (const unsigned short*)mmio_r32(TEXT_CELLS_BASE);
    CHECK_EQ((int)cells[0], 0x4141);                        // the attribute is the cell's high byte
    CHECK_EQ((int)cells[1], 0x0762);                        // 0 is the terminal's own: light grey (7)
    text_shutdown();
    return test_summary();
}
