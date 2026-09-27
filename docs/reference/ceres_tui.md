# `<ceres/tui.h>`

A small text user interface, drawn into the GPU's text plane (text.h): windows with a title, labels, buttons, a progress bar, a scrolling list and a menu that runs on the terminal's keys. Nothing here owns the screen. tui_init() sets up the grid, the drawing calls change it, and tui_present() shows the frame, so a program is free to mix these with text_* calls.

COLOUR is the text plane's attribute (a byte, TEXT_ATTR(ink, background)); the theme below is what the pieces are drawn with, and a program can draw its own with text_set_attr() first.

KEYS. tui_menu() reads keystrokes (ceres/key.h): w, k or the up arrow move up; s, j or the down arrow move down; Home and End go to the first and the last; PageUp and PageDown move by a screenful; Enter or Space choose; q, Escape, or the end of the input cancel. Every key redraws and shows the frame. It asks the host for keys as they are pressed for as long as it runs, so on a console and in the window they act at once and the arrows work; from a file the same keys are read as the bytes a terminal sends.

```c
#define TUI_NORMAL    TEXT_NORMAL                                // the terminal's own colours
#define TUI_FRAME     TEXT_ATTR(TEXT_CYAN, TEXT_BLACK)                 // the border of a window
#define TUI_TITLE     TEXT_ATTR(TEXT_BRIGHT + TEXT_YELLOW, TEXT_BLACK)   // its title
#define TUI_SELECTED  TEXT_ATTR(TEXT_BLACK, TEXT_WHITE)                // the item or button that has the focus
#define TUI_BAR_FULL  TEXT_ATTR(TEXT_BLACK, TEXT_GREEN)                // the done part of a progress bar
#define TUI_BAR_EMPTY TEXT_ATTR(TEXT_WHITE, TEXT_BLUE)                 // the rest
#define TUI_STATUS    TEXT_ATTR(TEXT_BLACK, TEXT_CYAN)                 // the status line

int  tui_init(int cols, int rows);              // text_init, cleared to blank; 0 ok, -1 if the grid does not fit
void tui_shutdown(void);
void tui_clear(void);                           // every cell blank, in the plain attribute
void tui_present(void);                         // shows the frame

// A box with `title` (may be NULL) set into its top edge; the inside is cleared. Needs w >= 4 and h >= 2.
void tui_window(int x, int y, int w, int h, const char* title);
void tui_label(int x, int y, const char* text);                          // in the plain attribute
void tui_label_center(int x, int y, int w, const char* text);            // centred in a run of w cells
void tui_button(int x, int y, const char* text, int focused);            // "[ text ]", highlighted when focused
void tui_progress(int x, int y, int w, int percent);                     // w cells (at least 6): a bar and " 42%"
void tui_status(const char* text);                                       // the whole bottom row, in the status colours

// A list of `count` items in a run of w cells and `rows` rows from (x, y), the first shown being items[top].
// The selected one is drawn highlighted. Returns the index of the first row shown, adjusted so that the
// selection is on screen: pass it back as `top` next time.
int  tui_list(int x, int y, int w, int rows, const char* const* items, int count, int selected, int top);

// A window with the title and the items, run until a choice is made: returns the index chosen, or -1 when
// the user cancels or the input ends. Blocks on the keys. Shows a frame at the start and after every key.
int  tui_menu(int x, int y, const char* title, const char* const* items, int count);
```
