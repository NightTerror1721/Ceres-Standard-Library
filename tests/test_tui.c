// The text user interface: the pieces are drawn and shown (the frames are in the expected output, escapes
// and all), then tui_menu runs on the keys in test_tui.stdin.
#include "ceres/test.h"
#include "ceres/tui.h"

int main(void)
{
    TEST_SECTION("init");
    CHECK_EQ(tui_init(0, 5), -1);
    CHECK_EQ(tui_init(30, 12), 0);
    CHECK_EQ(fb_cols(), 30);
    CHECK_EQ(fb_rows(), 12);

    TEST_SECTION("a window");
    tui_window(1, 1, 22, 5, "Settings");
    CHECK_EQ(fb_get(1, 1), '+');
    CHECK_EQ(fb_get(3, 1), 'S');                          // the title, set into the top edge
    CHECK_EQ((int)fb_get_attr(3, 1), TUI_TITLE);
    CHECK_EQ((int)fb_get_attr(1, 1), TUI_FRAME);
    CHECK_EQ(fb_get(5, 3), ' ');                          // the inside is clear
    tui_label(3, 2, "volume");
    tui_progress(3, 3, 18, 60);
    tui_label_center(2, 4, 20, "ok");
    tui_present();

    TEST_SECTION("buttons and a status line");
    tui_clear();
    tui_button(2, 2, "OK", 1);
    tui_button(10, 2, "Cancel", 0);
    CHECK_EQ((int)fb_get_attr(2, 2), TUI_SELECTED);       // the focused one is highlighted
    CHECK_EQ((int)fb_get_attr(10, 2), TUI_NORMAL);
    tui_status("ready");
    CHECK_EQ(fb_get(0, 11), 'r');
    CHECK_EQ((int)fb_get_attr(29, 11), TUI_STATUS);       // the whole row
    tui_present();

    TEST_SECTION("progress");
    tui_clear();
    tui_progress(0, 0, 20, 0);
    tui_progress(0, 1, 20, 100);
    tui_progress(0, 2, 20, 33);
    tui_progress(0, 3, 20, 250);                          // clamped
    tui_progress(0, 4, 20, -5);
    tui_progress(0, 5, 5, 50);                            // too narrow: nothing
    CHECK_EQ(fb_get(0, 5), ' ');
    tui_present();

    TEST_SECTION("a list scrolls to keep the selection on screen");
    const char* items[] = { "one", "two", "three", "four", "five", "six" };
    tui_clear();
    int top = tui_list(1, 1, 8, 3, items, 6, 0, 0);
    CHECK_EQ(top, 0);
    top = tui_list(1, 1, 8, 3, items, 6, 4, top);         // 'five' is below the three rows: it scrolls
    CHECK_EQ(top, 2);
    CHECK_EQ((int)fb_get_attr(1, 3), TUI_SELECTED);       // the last row shown
    top = tui_list(1, 1, 8, 3, items, 6, 1, top);         // back up
    CHECK_EQ(top, 1);
    tui_present();

    TEST_SECTION("a menu on the terminal keys");
    tui_clear();
    const char* colours[] = { "red", "green", "blue" };
    CHECK_EQ(tui_menu(2, 1, "Colour", colours, 3), 1);    // s, then enter
    CHECK_EQ(tui_menu(2, 1, "Colour", colours, 3), 2);    // j, the down arrow, and space; it stops at the last item
    CHECK_EQ(tui_menu(2, 1, "Colour", colours, 3), 0);    // w and k at the top stay there; enter
    CHECK_EQ(tui_menu(2, 1, "Colour", colours, 3), -1);   // q
    CHECK_EQ(tui_menu(2, 1, "Colour", colours, 3), -1);   // a lone Escape
    CHECK_EQ(tui_menu(2, 1, "Colour", colours, 3), -1);   // the input ends
    CHECK_EQ(tui_menu(2, 1, "Colour", colours, 0), -1);   // nothing to choose from
    tui_shutdown();
    return test_summary();
}
