#include "ceres/tui.h"
#include "stdio.h"
#include "string.h"
#include "ceres/key.h"

int tui_init(int cols, int rows)
{
    if (fb_init(cols, rows) != 0)
        return -1;
    fb_set_attr(TUI_NORMAL);
    fb_clear(' ');
    return 0;
}

void tui_shutdown(void)
{
    fb_set_attr(TUI_NORMAL);
    fb_shutdown();
}

void tui_clear(void)
{
    fb_set_attr(TUI_NORMAL);
    fb_clear(' ');
}

void tui_present(void)
{
    fb_present();
}

// Sets the attribute for the drawing that follows and returns the one it replaced, to be put back.
static unsigned char use(unsigned char attr)
{
    unsigned char before = fb_attr();
    fb_set_attr(attr);
    return before;
}

void tui_window(int x, int y, int w, int h, const char* title)
{
    if (w < 4 || h < 2)
        return;
    unsigned char before = use(TUI_NORMAL);
    fb_fill(x, y, w, h, ' ');                             // the inside, and whatever was under the frame
    fb_set_attr(TUI_FRAME);
    fb_box(x, y, w, h);
    if (title != 0 && title[0] != 0)
    {
        int room = w - 6;                                 // "+ title +" needs the corners and a space each side
        int length = (int)strlen(title);
        if (length > room)
            length = room;
        if (length > 0)
        {
            fb_put(x + 1, y, ' ');
            fb_set_attr(TUI_TITLE);
            for (int i = 0; i < length; i++)
                fb_put(x + 2 + i, y, title[i]);
            fb_set_attr(TUI_FRAME);
            fb_put(x + 2 + length, y, ' ');
        }
    }
    fb_set_attr(before);
}

void tui_label(int x, int y, const char* text)
{
    unsigned char before = use(TUI_NORMAL);
    fb_text(x, y, text);
    fb_set_attr(before);
}

void tui_label_center(int x, int y, int w, const char* text)
{
    int length = (int)strlen(text);
    int start = length >= w ? 0 : (w - length) / 2;
    unsigned char before = use(TUI_NORMAL);
    for (int i = 0; i < w && i < length; i++)
        fb_put(x + start + i, y, text[i]);
    fb_set_attr(before);
}

void tui_button(int x, int y, const char* text, int focused)
{
    unsigned char before = use(focused ? TUI_SELECTED : TUI_NORMAL);
    fb_put(x, y, '[');
    fb_put(x + 1, y, ' ');
    int length = (int)strlen(text);
    for (int i = 0; i < length; i++)
        fb_put(x + 2 + i, y, text[i]);
    fb_put(x + 2 + length, y, ' ');
    fb_put(x + 3 + length, y, ']');
    fb_set_attr(before);
}

void tui_progress(int x, int y, int w, int percent)
{
    if (w < 6)
        return;
    if (percent < 0)
        percent = 0;
    if (percent > 100)
        percent = 100;
    int bar = w - 5;                                      // " 100%" is five cells
    int done = bar * percent / 100;
    unsigned char before = fb_attr();
    for (int i = 0; i < bar; i++)
    {
        fb_set_attr(i < done ? TUI_BAR_FULL : TUI_BAR_EMPTY);
        fb_put(x + i, y, i < done ? '#' : '-');
    }
    fb_set_attr(TUI_NORMAL);
    char text[8];
    snprintf(text, sizeof(text), "%4d%%", percent);
    fb_text(x + bar, y, text);
    fb_set_attr(before);
}

void tui_status(const char* text)
{
    int row = fb_rows() - 1;
    int cols = fb_cols();
    if (row < 0)
        return;
    unsigned char before = use(TUI_STATUS);
    fb_hline(0, row, cols, ' ');
    for (int i = 0; i < cols && text[i] != 0; i++)
        fb_put(i, row, text[i]);
    fb_set_attr(before);
}

int tui_list(int x, int y, int w, int rows, const char* const* items, int count, int selected, int top)
{
    if (rows < 1 || w < 1)
        return top;
    if (selected < top)
        top = selected;
    if (selected >= top + rows)
        top = selected - rows + 1;
    if (top < 0)
        top = 0;
    unsigned char before = fb_attr();
    for (int r = 0; r < rows; r++)
    {
        int index = top + r;
        fb_set_attr(index == selected ? TUI_SELECTED : TUI_NORMAL);
        fb_hline(x, y + r, w, ' ');
        if (index < count)
            for (int i = 0; i < w && items[index][i] != 0; i++)
                fb_put(x + i, y + r, items[index][i]);
    }
    fb_set_attr(before);
    return top;
}

int tui_menu(int x, int y, const char* title, const char* const* items, int count)
{
    if (count < 1)
        return -1;
    int widest = title != 0 ? (int)strlen(title) + 2 : 0;    // the title sits in the top edge with a space each side
    for (int i = 0; i < count; i++)
    {
        int length = (int)strlen(items[i]);
        if (length > widest)
            widest = length;
    }
    int w = widest + 4;                                       // a border and a space on each side
    int rows = count;
    if (y + rows + 2 > fb_rows())
        rows = fb_rows() - y - 2;
    if (rows < 1 || x + w > fb_cols())
        return -1;

    int selected = 0;
    int top = 0;
    int chosen = -1;
    key_start();                                              // keys as they are pressed, for as long as the menu runs
    for (;;)
    {
        tui_window(x, y, w, rows + 2, title);
        top = tui_list(x + 2, y + 1, w - 4, rows, items, count, selected, top);
        tui_status("arrows or w/s: move   enter: choose   esc or q: cancel");
        tui_present();

        int key = key_wait();
        if (key == KEYC_NONE || key == 'q' || key == KEYC_ESC)
            break;                                            // cancelled, or the input ended
        if ((key == 'w' || key == 'k' || key == KEYC_UP) && selected > 0)
            selected--;
        else if ((key == 's' || key == 'j' || key == KEYC_DOWN) && selected < count - 1)
            selected++;
        else if (key == KEYC_HOME)
            selected = 0;
        else if (key == KEYC_END)
            selected = count - 1;
        else if (key == KEYC_PAGEUP)
            selected = selected > rows ? selected - rows : 0;
        else if (key == KEYC_PAGEDOWN)
            selected = selected + rows < count ? selected + rows : count - 1;
        else if (key == KEYC_ENTER || key == ' ')
        {
            chosen = selected;
            break;
        }
    }
    key_stop();
    return chosen;
}
