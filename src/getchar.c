// getchar and getchar_nb. getchar() goes through stdin (fgetc), which is the stream scanf and ungetc use, so a
// program that mixes them sees one input stream, as the standard wants. getchar_nb() reads the terminal's buffer
// directly (file.c's __stdin_getc_nb) and is the loop-friendly form.
#include "stdio.h"

int __stdin_getc_nb(void);                            // file.c: the pushed-back character, else a waiting byte

int getchar(void)
{
    return fgetc(stdin);                              // the same stream scanf reads, so ungetc/scanf/getchar agree
}

int getchar_nb(void)
{
    return __stdin_getc_nb();                         // -1 when nothing is buffered
}
