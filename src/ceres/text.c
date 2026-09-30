// The GPU's text plane. See ceres/text.h.
#include "ceres/text.h"
#include "ceres/terminal.h"
#include "ceres/utf8.h"
#include "stdlib.h"
#include "stdarg.h"
#include "stdio.h"
#include "string.h"

static unsigned short* grid;            // a cell a short: the character's byte, and its attribute in the high byte
static int cols;
static int rows;
static unsigned char current_attr;      // what drawing gives the cells it writes (0: TEXT_NORMAL)

// The code points of glyphs 0x80-0x9F: the box and block characters the GPU's font has where Latin-1 has controls.
static const unsigned short box_code_points[32] = {
    0x2500, 0x2502, 0x250C, 0x2510, 0x2514, 0x2518, 0x251C, 0x2524,
    0x252C, 0x2534, 0x253C, 0x2550, 0x2551, 0x2554, 0x2557, 0x255A,
    0x255D, 0x2560, 0x2563, 0x2566, 0x2569, 0x256C, 0x2588, 0x2580,
    0x2584, 0x2591, 0x2592, 0x2593, 0x25A0, 0x2022, 0x25B2, 0x25BC,
};

#define BOX_H   0x80   // ─
#define BOX_V   0x81   // │
#define BOX_TL  0x82   // ┌
#define BOX_TR  0x83   // ┐
#define BOX_BL  0x84   // └
#define BOX_BR  0x85   // ┘

// The attribute is kept as given: 0 is the normal colours, which the screen's cell gets when it is drawn.
static unsigned short cell_of(char c, unsigned char attr)
{
    return (unsigned short)((unsigned char)c | ((unsigned int)attr << 8));
}

#define NORMAL_CELL_ATTR 0x07u   // light grey on what is behind: the terminal's own colours

int text_screen_cols(void) { return (int)mmio_r32(TEXT_COLS); }
int text_screen_rows(void) { return (int)mmio_r32(TEXT_ROWS); }

int text_init(int width, int height)
{
    if (width < 1 || height < 1 || width > text_screen_cols() || height > text_screen_rows())
        return -1;
    unsigned short* fresh = (unsigned short*)malloc((size_t)width * (size_t)height * sizeof(unsigned short));
    if (fresh == NULL)
        return -1;
    free(grid);
    grid = fresh;
    cols = width;
    rows = height;
    current_attr = 0;
    for (int i = 0; i < cols * rows; i++)
        grid[i] = cell_of(' ', 0);
    term_show_cursor(0);
    return 0;
}

void text_shutdown(void)
{
    free(grid);
    grid = NULL;
    cols = 0;
    rows = 0;
    term_show_cursor(1);
}

int text_cols(void) { return cols; }
int text_rows(void) { return rows; }

void text_set_attr(unsigned char attr) { current_attr = attr; }
unsigned char text_attr(void) { return current_attr; }

void text_clear(char c)
{
    if (grid == NULL)
        return;
    const unsigned short cell = cell_of(c, current_attr);
    for (int i = 0; i < cols * rows; i++)
        grid[i] = cell;
}

// The grid, a row at a time (the screen's rows are wider), into the cells in VRAM; then the blank that shows it.
void text_present(void)
{
    if (grid == NULL)
        return;
    unsigned short* base = (unsigned short*)mmio_r32(TEXT_CELLS_BASE);
    const int stride = text_screen_cols();
    for (int y = 0; y < rows; y++)
    {
        const unsigned short* from = grid + y * cols;
        unsigned short* to = base + y * stride;
        int x = 0;
        for (; x + 1 < cols && (((unsigned int)(to + x) & 3u) == 0); x += 2)
        {
            unsigned int a = from[x], b = from[x + 1];   // two cells in one 32-bit VRAM store
            if ((a >> 8) == 0) a |= (unsigned int)NORMAL_CELL_ATTR << 8;
            if ((b >> 8) == 0) b |= (unsigned int)NORMAL_CELL_ATTR << 8;
            *(unsigned int*)(to + x) = a | (b << 16);
        }
        for (; x < cols; x++)
        {
            unsigned int a = from[x];
            if ((a >> 8) == 0) a |= (unsigned int)NORMAL_CELL_ATTR << 8;
            to[x] = (unsigned short)a;
        }
    }
    video_present();
    video_wait_present();
}

void text_put_attr(int x, int y, char c, unsigned char attr)
{
    if (grid != NULL && x >= 0 && x < cols && y >= 0 && y < rows)
        grid[y * cols + x] = cell_of(c, attr);
}

unsigned char text_get_attr(int x, int y)
{
    if (grid != NULL && x >= 0 && x < cols && y >= 0 && y < rows)
        return (unsigned char)(grid[y * cols + x] >> 8);
    return 0;
}

void text_put(int x, int y, char c)
{
    text_put_attr(x, y, c, current_attr);
}

char text_get(int x, int y)
{
    if (grid != NULL && x >= 0 && x < cols && y >= 0 && y < rows)
        return (char)(grid[y * cols + x] & 0xFFu);
    return ' ';
}

unsigned char text_glyph(unsigned int cp)
{
    if ((cp >= 0x20u && cp < 0x7Fu) || (cp >= 0xA0u && cp <= 0xFFu))
        return (unsigned char)cp;
    for (unsigned int i = 0; i < 32u; i++)
        if (box_code_points[i] == cp)
            return (unsigned char)(0x80u + i);
    return '?';
}

void text_put_char(int x, int y, unsigned int cp)
{
    text_put(x, y, (char)text_glyph(cp));
}

void text_text(int x, int y, const char* s)
{
    int cx = x;
    unsigned int cp;
    while ((cp = utf8_next(&s)) != 0)
    {
        if (cp == '\n')
        {
            cx = x;
            y++;
            continue;
        }
        text_put_char(cx, y, cp);
        cx++;
    }
}

int text_text_n(int x, int y, const char* s, int max)
{
    int n = 0;
    unsigned int cp;
    while (n < max && (cp = utf8_next(&s)) != 0 && cp != '\n')
        text_put_char(x + n++, y, cp);
    return n;
}

int text_text_width(const char* s)
{
    int widest = 0, line = 0;
    unsigned int cp;
    while ((cp = utf8_next(&s)) != 0)
    {
        line = cp == '\n' ? 0 : line + 1;
        if (line > widest)
            widest = line;
    }
    return widest;
}

void text_printf(int x, int y, const char* fmt, ...)
{
    char line[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    text_text(x, y, line);
}

void text_hline(int x, int y, int len, char c)
{
    for (int i = 0; i < len; i++)
        text_put(x + i, y, c);
}

void text_vline(int x, int y, int len, char c)
{
    for (int i = 0; i < len; i++)
        text_put(x, y + i, c);
}

void text_fill(int x, int y, int w, int h, char c)
{
    for (int j = 0; j < h; j++)
        text_hline(x, y + j, w, c);
}

void text_rect(int x, int y, int w, int h, char c)
{
    if (w < 1 || h < 1)
        return;
    text_hline(x, y, w, c);
    text_hline(x, y + h - 1, w, c);
    text_vline(x, y, h, c);
    text_vline(x + w - 1, y, h, c);
}

void text_box(int x, int y, int w, int h)
{
    if (w < 2 || h < 2)
        return;
    text_hline(x + 1, y, w - 2, (char)BOX_H);
    text_hline(x + 1, y + h - 1, w - 2, (char)BOX_H);
    text_vline(x, y + 1, h - 2, (char)BOX_V);
    text_vline(x + w - 1, y + 1, h - 2, (char)BOX_V);
    text_put(x, y, (char)BOX_TL);
    text_put(x + w - 1, y, (char)BOX_TR);
    text_put(x, y + h - 1, (char)BOX_BL);
    text_put(x + w - 1, y + h - 1, (char)BOX_BR);
}

void text_copy(int dx, int dy, int sx, int sy, int w, int h)
{
    if (grid == 0 || w < 1 || h < 1)
        return;
    // Copy in the direction that never reads a cell it has already overwritten.
    int first_row = 0, last_row = h, row_step = 1;
    if (dy > sy)
    {
        first_row = h - 1;
        last_row = -1;
        row_step = -1;
    }
    int first_col = 0, last_col = w, col_step = 1;
    if (dx > sx)
    {
        first_col = w - 1;
        last_col = -1;
        col_step = -1;
    }
    for (int j = first_row; j != last_row; j += row_step)
        for (int i = first_col; i != last_col; i += col_step)
            text_put_attr(dx + i, dy + j, text_get(sx + i, sy + j), text_get_attr(sx + i, sy + j));
}

void text_scroll(int dy)
{
    if (grid == 0 || dy == 0)
        return;
    if (dy >= rows || dy <= -rows)
    {
        text_clear(' ');
        return;
    }
    const unsigned short blank = cell_of(' ', current_attr);
    const size_t row = (size_t)cols;
    if (dy > 0)
    {
        memmove(grid, grid + dy * cols, (size_t)(rows - dy) * row * sizeof(unsigned short));
        for (int i = (rows - dy) * cols; i < rows * cols; i++)
            grid[i] = blank;
    }
    else
    {
        int n = -dy;
        memmove(grid + n * cols, grid, (size_t)(rows - n) * row * sizeof(unsigned short));
        for (int i = 0; i < n * cols; i++)
            grid[i] = blank;
    }
}
