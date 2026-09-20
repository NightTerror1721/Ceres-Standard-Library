// ANSI escape sequences. See ceres/ansi.h.
#include "ceres/ansi.h"
#include "stdio.h"

// ESC [ <n> <letter>, with the count left out when it is 1 (the default) is not worth the special case.
static void csi_n(int n, char letter)
{
    putstr("\x1b[");
    putuint((unsigned int)(n < 0 ? 0 : n));
    putchar(letter);
}

void ansi_goto(int col, int row)
{
    putstr("\x1b[");
    putuint((unsigned int)(row < 1 ? 1 : row));
    putchar(';');
    putuint((unsigned int)(col < 1 ? 1 : col));
    putchar('H');
}

void ansi_fg(int color) { csi_n(30 + (color & 7), 'm'); }
void ansi_bg(int color) { csi_n(40 + (color & 7), 'm'); }

void ansi_fg256(int n)
{
    putstr("\x1b[38;5;");
    putuint((unsigned int)(n & 255));
    putchar('m');
}

void ansi_bg256(int n)
{
    putstr("\x1b[48;5;");
    putuint((unsigned int)(n & 255));
    putchar('m');
}

void ansi_up(int n)    { if (n > 0) csi_n(n, 'A'); }
void ansi_down(int n)  { if (n > 0) csi_n(n, 'B'); }
void ansi_right(int n) { if (n > 0) csi_n(n, 'C'); }
void ansi_left(int n)  { if (n > 0) csi_n(n, 'D'); }

void ansi_clear_line(void)     { putstr("\x1b[2K"); }
void ansi_save_cursor(void)    { putstr("\x1b[s"); }
void ansi_restore_cursor(void) { putstr("\x1b[u"); }
