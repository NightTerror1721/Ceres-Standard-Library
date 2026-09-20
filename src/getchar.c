// getchar and getchar_nb, kept apart from the rest of the console code (stdio.c) so that a program that only
// prints does not link the FILE layer they read through.
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
