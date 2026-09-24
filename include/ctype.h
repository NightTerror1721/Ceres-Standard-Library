#pragma once

// Every one looks at its argument alone (the "C" locale), so each is `const`: a call whose result
// nothing reads may go.

int isalnum(int c) __attribute__((const));

int isalpha(int c) __attribute__((const));

int isblank(int c) __attribute__((const));

int iscntrl(int c) __attribute__((const));

int isdigit(int c) __attribute__((const));

int isgraph(int c) __attribute__((const));

int islower(int c) __attribute__((const));

int isprint(int c) __attribute__((const));

int ispunct(int c) __attribute__((const));

int isspace(int c) __attribute__((const));

int isupper(int c) __attribute__((const));

int isxdigit(int c) __attribute__((const));

int tolower(int c) __attribute__((const));

int toupper(int c) __attribute__((const));

// Not ISO C: from BSD and POSIX.
int isascii(int c) __attribute__((const));               // c is in 0..127
int toascii(int c) __attribute__((const));               // c with all but the low seven bits cleared
