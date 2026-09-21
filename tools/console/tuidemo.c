// A menu run twice on keys as they are pressed, then an ordinary line read: what a console user would do.
#include "ceres/tui.h"
#include <stdio.h>

int main(void)
{
    tui_init(40, 10);
    const char* items[] = { "red", "green", "blue", "cyan" };
    int a = tui_menu(2, 1, "Colour", items, 4);
    int b = tui_menu(2, 1, "Again", items, 4);
    tui_shutdown();
    printf("\nchosen %d %d\n", a, b);
    char line[40];
    printf("type a line: ");
    if (fgets(line, sizeof line, stdin))
        printf("line [%s]\n", line);
    return 0;
}
