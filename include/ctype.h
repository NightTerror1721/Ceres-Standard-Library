#pragma once

// Every one looks at its argument alone (the "C" locale), so each is `const`: a call whose result
// nothing reads may go.

int isalnum(int c) __attribute__((__const__));

int isalpha(int c) __attribute__((__const__));

int isblank(int c) __attribute__((__const__));

int iscntrl(int c) __attribute__((__const__));

int isdigit(int c) __attribute__((__const__));

int isgraph(int c) __attribute__((__const__));

int islower(int c) __attribute__((__const__));

int isprint(int c) __attribute__((__const__));

int ispunct(int c) __attribute__((__const__));

int isspace(int c) __attribute__((__const__));

int isupper(int c) __attribute__((__const__));

int isxdigit(int c) __attribute__((__const__));

int tolower(int c) __attribute__((__const__));

int toupper(int c) __attribute__((__const__));

// Not ISO C: from BSD and POSIX.
int isascii(int c) __attribute__((__const__));               // c is in 0..127
int toascii(int c) __attribute__((__const__));               // c with all but the low seven bits cleared
