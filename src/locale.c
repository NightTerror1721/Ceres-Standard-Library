// The "C" locale, the only one there is. See locale.h.
#include "locale.h"
#include "string.h"

static char c_name[] = "C";
static char empty[] = "";
static char dot[] = ".";

// CHAR_MAX (127) is the standard's "not available" for the numeric fields of a locale that has no such thing; the
// fields are filled in by the first localeconv() rather than by an initializer.
static struct lconv c_locale;
static int c_locale_ready;

char* setlocale(int category, const char* locale)
{
    if (category < LC_ALL || category > LC_TIME)
        return 0;
    if (locale == 0 || locale[0] == 0 || strcmp(locale, "C") == 0 || strcmp(locale, "POSIX") == 0 ||
        strcmp(locale, "C.UTF-8") == 0 || strcmp(locale, "C.utf8") == 0)
        return c_name;                                   // a query, or a request for what there is
    return 0;
}

struct lconv* localeconv(void)
{
    if (!c_locale_ready)
    {
        c_locale.decimal_point = dot;
        c_locale.thousands_sep = empty;
        c_locale.grouping = empty;
        c_locale.mon_decimal_point = empty;
        c_locale.mon_thousands_sep = empty;
        c_locale.mon_grouping = empty;
        c_locale.positive_sign = empty;
        c_locale.negative_sign = empty;
        c_locale.currency_symbol = empty;
        c_locale.int_curr_symbol = empty;
        c_locale.frac_digits = 127;
        c_locale.p_cs_precedes = 127;
        c_locale.n_cs_precedes = 127;
        c_locale.p_sep_by_space = 127;
        c_locale.n_sep_by_space = 127;
        c_locale.p_sign_posn = 127;
        c_locale.n_sign_posn = 127;
        c_locale.int_frac_digits = 127;
        c_locale.int_p_cs_precedes = 127;
        c_locale.int_n_cs_precedes = 127;
        c_locale.int_p_sep_by_space = 127;
        c_locale.int_n_sep_by_space = 127;
        c_locale.int_p_sign_posn = 127;
        c_locale.int_n_sign_posn = 127;
        c_locale_ready = 1;
    }
    return &c_locale;
}
