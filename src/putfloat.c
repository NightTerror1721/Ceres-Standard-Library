// putfloat, kept apart from stdio.c: it goes through the format engine, and a program that only puts strings and
// integers should not carry the engine (about 10 KB, mostly floating-point printing).
#include "stdio.h"

int putfloat(float f, int decimals)
{
    if (decimals < 0) decimals = 0;
    if (decimals > 9) decimals = 9;
    return printf("%.*f", decimals, f);
}
