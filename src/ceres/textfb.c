#include "ceres/textfb.h"
#include "ceres/utf8.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"

static char* grid;
static unsigned char* attrs;                 // one attribute per cell, alongside the characters
static int cols;
static int rows;
static unsigned char current_attr;           // what drawing gives the cells it writes
static int attrs_used;                       // some cell was given a non-zero attribute since the last clear

#define FB_BLOCK_CMD_ATTRS 3

int fb_init(int width, int height)
{
    if (width < 1 || height < 1 || width > FB_MAX_COLS || height > FB_MAX_ROWS)
        return -1;
    // The device ignores a size it does not accept and keeps the old one, so ask and read it back.
    mmio_w32(FB_WIDTH, (unsigned int)width);
    mmio_w32(FB_HEIGHT, (unsigned int)height);
    if ((int)mmio_r32(FB_WIDTH) != width || (int)mmio_r32(FB_HEIGHT) != height)
        return -1;

    char* fresh = (char*)malloc((size_t)width * (size_t)height);
    unsigned char* fresh_attrs = (unsigned char*)malloc((size_t)width * (size_t)height);
    if (fresh == 0 || fresh_attrs == 0)
    {
        free(fresh);
        free(fresh_attrs);
        return -1;
    }
    free(grid);
    free(attrs);
    grid = fresh;
    attrs = fresh_attrs;
    cols = width;
    rows = height;
    memset(grid, ' ', (size_t)cols * (size_t)rows);
    memset(attrs, 0, (size_t)cols * (size_t)rows);
    attrs_used = 0;
    return 0;
}

void fb_shutdown(void)
{
    free(grid);
    free(attrs);
    grid = 0;
    attrs = 0;
    cols = 0;
    rows = 0;
    attrs_used = 0;
}

void fb_set_output(int output)
{
    if (output == FB_OUT_AUTO || output == FB_OUT_TERMINAL || output == FB_OUT_WINDOW)
        mmio_w32(FB_MODE, (unsigned int)output);
}

int fb_output(void)
{
    return (int)mmio_r32(FB_OUTPUT);
}

int fb_cols(void) { return cols; }
int fb_rows(void) { return rows; }

void fb_set_attr(unsigned char attr)
{
    current_attr = attr;
}

unsigned char fb_attr(void)
{
    return current_attr;
}

void fb_clear(char c)
{
    if (grid != 0)
    {
        memset(grid, c, (size_t)cols * (size_t)rows);
        memset(attrs, current_attr, (size_t)cols * (size_t)rows);
        attrs_used = current_attr != 0;
    }
}

void fb_present(void)
{
    if (grid == 0)
        return;
    mmio_w32(FB_CMD, FB_CMD_CLEAR);                       // rewinds the device's write cursor
    mmio_w32(FB_BLOCK_ADDR, (unsigned int)grid);
    mmio_w32(FB_BLOCK_LEN, (unsigned int)(cols * rows));
    mmio_w32(FB_BLOCK_CMD, 2);                            // the whole grid in one transfer
    if (attrs_used)
    {
        mmio_w32(FB_BLOCK_ADDR, (unsigned int)attrs);     // and, if anything is coloured, its attributes in another
        mmio_w32(FB_BLOCK_LEN, (unsigned int)(cols * rows));
        mmio_w32(FB_BLOCK_CMD, FB_BLOCK_CMD_ATTRS);
    }
    mmio_w32(FB_CMD, FB_CMD_PRESENT);
}

void fb_put_attr(int x, int y, char c, unsigned char attr)
{
    if (grid != 0 && x >= 0 && x < cols && y >= 0 && y < rows)
    {
        grid[y * cols + x] = c;
        attrs[y * cols + x] = attr;
        if (attr != 0)
            attrs_used = 1;
    }
}

unsigned char fb_get_attr(int x, int y)
{
    if (grid != 0 && x >= 0 && x < cols && y >= 0 && y < rows)
        return attrs[y * cols + x];
    return 0;
}

void fb_put(int x, int y, char c)
{
    fb_put_attr(x, y, c, current_attr);
}

char fb_get(int x, int y)
{
    if (grid != 0 && x >= 0 && x < cols && y >= 0 && y < rows)
        return grid[y * cols + x];
    return ' ';
}

void fb_put_char(int x, int y, unsigned int cp)
{
    fb_put(x, y, cp <= 0xFFu ? (char)cp : '?');       // a cell is Latin-1
}

void fb_text(int x, int y, const char* s)
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
        fb_put_char(cx, y, cp);
        cx++;
    }
}

int fb_text_n(int x, int y, const char* s, int max)
{
    int n = 0;
    unsigned int cp;
    while (n < max && (cp = utf8_next(&s)) != 0 && cp != '\n')
        fb_put_char(x + n++, y, cp);
    return n;
}

int fb_text_width(const char* s)
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

void fb_printf(int x, int y, const char* fmt, ...)
{
    char line[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    fb_text(x, y, line);
}

void fb_hline(int x, int y, int len, char c)
{
    for (int i = 0; i < len; i++)
        fb_put(x + i, y, c);
}

void fb_vline(int x, int y, int len, char c)
{
    for (int i = 0; i < len; i++)
        fb_put(x, y + i, c);
}

void fb_fill(int x, int y, int w, int h, char c)
{
    for (int j = 0; j < h; j++)
        fb_hline(x, y + j, w, c);
}

void fb_rect(int x, int y, int w, int h, char c)
{
    if (w < 1 || h < 1)
        return;
    fb_hline(x, y, w, c);
    fb_hline(x, y + h - 1, w, c);
    fb_vline(x, y, h, c);
    fb_vline(x + w - 1, y, h, c);
}

void fb_box(int x, int y, int w, int h)
{
    if (w < 2 || h < 2)
        return;
    fb_hline(x + 1, y, w - 2, '-');
    fb_hline(x + 1, y + h - 1, w - 2, '-');
    fb_vline(x, y + 1, h - 2, '|');
    fb_vline(x + w - 1, y + 1, h - 2, '|');
    fb_put(x, y, '+');
    fb_put(x + w - 1, y, '+');
    fb_put(x, y + h - 1, '+');
    fb_put(x + w - 1, y + h - 1, '+');
}

void fb_copy(int dx, int dy, int sx, int sy, int w, int h)
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
            fb_put_attr(dx + i, dy + j, fb_get(sx + i, sy + j), fb_get_attr(sx + i, sy + j));
}

void fb_scroll(int dy)
{
    if (grid == 0 || dy == 0)
        return;
    if (dy >= rows || dy <= -rows)
    {
        fb_clear(' ');
        return;
    }
    if (dy > 0)
    {
        memmove(grid, grid + dy * cols, (size_t)((rows - dy) * cols));
        memmove(attrs, attrs + dy * cols, (size_t)((rows - dy) * cols));
        memset(grid + (rows - dy) * cols, ' ', (size_t)(dy * cols));
        memset(attrs + (rows - dy) * cols, current_attr, (size_t)(dy * cols));
    }
    else
    {
        int n = -dy;
        memmove(grid + n * cols, grid, (size_t)((rows - n) * cols));
        memmove(attrs + n * cols, attrs, (size_t)((rows - n) * cols));
        memset(grid, ' ', (size_t)(n * cols));
        memset(attrs, current_attr, (size_t)(n * cols));
    }
    if (current_attr != 0)
        attrs_used = 1;
}
