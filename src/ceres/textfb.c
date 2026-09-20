#include "ceres/textfb.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"

static char* grid;
static int cols;
static int rows;

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
    if (fresh == 0)
        return -1;
    free(grid);
    grid = fresh;
    cols = width;
    rows = height;
    memset(grid, ' ', (size_t)cols * (size_t)rows);
    return 0;
}

void fb_shutdown(void)
{
    free(grid);
    grid = 0;
    cols = 0;
    rows = 0;
}

int fb_cols(void) { return cols; }
int fb_rows(void) { return rows; }

void fb_clear(char c)
{
    if (grid != 0)
        memset(grid, c, (size_t)cols * (size_t)rows);
}

void fb_present(void)
{
    if (grid == 0)
        return;
    mmio_w32(FB_CMD, FB_CMD_CLEAR);                       // rewinds the device's write cursor
    mmio_w32(FB_BLOCK_ADDR, (unsigned int)grid);
    mmio_w32(FB_BLOCK_LEN, (unsigned int)(cols * rows));
    mmio_w32(FB_BLOCK_CMD, 2);                            // the whole grid in one transfer
    mmio_w32(FB_CMD, FB_CMD_PRESENT);
}

void fb_put(int x, int y, char c)
{
    if (grid != 0 && x >= 0 && x < cols && y >= 0 && y < rows)
        grid[y * cols + x] = c;
}

char fb_get(int x, int y)
{
    if (grid != 0 && x >= 0 && x < cols && y >= 0 && y < rows)
        return grid[y * cols + x];
    return ' ';
}

void fb_text(int x, int y, const char* s)
{
    int cx = x;
    for (int i = 0; s[i] != 0; i++)
    {
        if (s[i] == '\n')
        {
            cx = x;
            y++;
            continue;
        }
        fb_put(cx, y, s[i]);
        cx++;
    }
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
            fb_put(dx + i, dy + j, fb_get(sx + i, sy + j));
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
        memset(grid + (rows - dy) * cols, ' ', (size_t)(dy * cols));
    }
    else
    {
        int n = -dy;
        memmove(grid + n * cols, grid, (size_t)((rows - n) * cols));
        memset(grid, ' ', (size_t)(n * cols));
    }
}
