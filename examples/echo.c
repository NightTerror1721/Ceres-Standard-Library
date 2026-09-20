// Echoes every line back with its length and stops at a line that says "quit".
//
// This is the terminal's input model in one page: getchar() WAITS for a byte, and the device has no
// end-of-input signal, so a program must give the user a way to say "done" (here: "quit") - it can
// never see EOF. Type at the terminal, or pipe the lines in:  (echo one; echo quit) | ceres run ...
#include "stdio.h"
#include "string.h"

#define LINE_MAX_CHARS 80

int main(void)
{
    char line[LINE_MAX_CHARS];
    puts("type a line and press Enter; \"quit\" ends the program");
    for (;;)
    {
        int n = 0;
        int c = getchar();
        while (c != '\n' && c != '\r')
        {
            if (n < LINE_MAX_CHARS - 1)
            {
                line[n] = (char)c;
                n++;
            }
            c = getchar();
        }
        line[n] = 0;
        if (strcmp(line, "quit") == 0)
            break;
        printf("[%d] %s\n", n, line);
    }
    puts("bye");
    return 0;
}
