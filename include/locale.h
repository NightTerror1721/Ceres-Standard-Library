#pragma once

// Locales. There is one, "C": setlocale() accepts it (and "" and "POSIX", which mean the same here) and
// refuses every other, and localeconv() describes it. Text is bytes and ctype.h is ASCII, so there is nothing
// for a locale to change.
#include "stddef.h"

#define LC_ALL       0
#define LC_COLLATE   1
#define LC_CTYPE     2
#define LC_MONETARY  3
#define LC_NUMERIC   4
#define LC_TIME      5

struct lconv
{
    char* decimal_point;          // "."
    char* thousands_sep;          // ""
    char* grouping;               // ""
    char* mon_decimal_point;      // ""
    char* mon_thousands_sep;      // ""
    char* mon_grouping;           // ""
    char* positive_sign;          // ""
    char* negative_sign;          // ""
    char* currency_symbol;        // ""
    char* int_curr_symbol;        // ""
    char frac_digits;             // CHAR_MAX: not available in the "C" locale
    char p_cs_precedes;
    char n_cs_precedes;
    char p_sep_by_space;
    char n_sep_by_space;
    char p_sign_posn;
    char n_sign_posn;
    char int_frac_digits;
    char int_p_cs_precedes;
    char int_n_cs_precedes;
    char int_p_sep_by_space;
    char int_n_sep_by_space;
    char int_p_sign_posn;
    char int_n_sign_posn;
};

char* setlocale(int category, const char* locale);   // "C" for NULL, "", "C" and "POSIX"; NULL for any other name or an unknown category
struct lconv* localeconv(void);                      // the "C" locale, always the same object
