// <ctype.h>: every classification and conversion over the whole range 0..255, and EOF, against what the "C"
// locale says. The classes are written out from their definitions here, apart from the library's code.
#include "ceres/test.h"
#include "ctype.h"

static int is_upper(int c) { return c >= 'A' && c <= 'Z'; }
static int is_lower(int c) { return c >= 'a' && c <= 'z'; }
static int is_digit(int c) { return c >= '0' && c <= '9'; }
static int is_space(int c) { return c == ' ' || (c >= 9 && c <= 13); }
static int is_cntrl(int c) { return (c >= 0 && c < 32) || c == 127; }
static int is_print(int c) { return c >= 32 && c < 127; }
static int is_punct(int c) { return is_print(c) && c != ' ' && !is_upper(c) && !is_lower(c) && !is_digit(c); }

int main(void)
{
    TEST_SECTION("each class, every value");
    int bad_alpha = 0, bad_alnum = 0, bad_upper = 0, bad_lower = 0, bad_digit = 0, bad_xdigit = 0;
    int bad_space = 0, bad_blank = 0, bad_cntrl = 0, bad_print = 0, bad_graph = 0, bad_punct = 0;
    for (int c = -1; c < 256; c++)
    {
        if ((isalpha(c) != 0) != (is_upper(c) || is_lower(c))) bad_alpha++;
        if ((isalnum(c) != 0) != (is_upper(c) || is_lower(c) || is_digit(c))) bad_alnum++;
        if ((isupper(c) != 0) != is_upper(c)) bad_upper++;
        if ((islower(c) != 0) != is_lower(c)) bad_lower++;
        if ((isdigit(c) != 0) != is_digit(c)) bad_digit++;
        if ((isxdigit(c) != 0) != (is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) bad_xdigit++;
        if ((isspace(c) != 0) != is_space(c)) bad_space++;
        if ((isblank(c) != 0) != (c == ' ' || c == '\t')) bad_blank++;
        if ((iscntrl(c) != 0) != is_cntrl(c)) bad_cntrl++;
        if ((isprint(c) != 0) != is_print(c)) bad_print++;
        if ((isgraph(c) != 0) != (is_print(c) && c != ' ')) bad_graph++;
        if ((ispunct(c) != 0) != is_punct(c)) bad_punct++;
    }
    CHECK_EQ(bad_alpha, 0);
    CHECK_EQ(bad_alnum, 0);
    CHECK_EQ(bad_upper, 0);
    CHECK_EQ(bad_lower, 0);
    CHECK_EQ(bad_digit, 0);
    CHECK_EQ(bad_xdigit, 0);
    CHECK_EQ(bad_space, 0);
    CHECK_EQ(bad_blank, 0);
    CHECK_EQ(bad_cntrl, 0);
    CHECK_EQ(bad_print, 0);
    CHECK_EQ(bad_graph, 0);
    CHECK_EQ(bad_punct, 0);

    TEST_SECTION("the classes partition the printable characters");
    int partition_ok = 1;
    for (int c = 0; c < 128; c++)
    {
        int classes = (isalpha(c) != 0) + (isdigit(c) != 0) + (ispunct(c) != 0) + (c == ' ') + (iscntrl(c) != 0);
        if (classes != 1)
            partition_ok = 0;                            // a character is exactly one of letter, digit, punctuation, space or control
    }
    CHECK(partition_ok);

    TEST_SECTION("tolower and toupper");
    int bad_lower_map = 0, bad_upper_map = 0;
    for (int c = -1; c < 256; c++)
    {
        if (tolower(c) != (is_upper(c) ? c + 32 : c)) bad_lower_map++;
        if (toupper(c) != (is_lower(c) ? c - 32 : c)) bad_upper_map++;
    }
    CHECK_EQ(bad_lower_map, 0);
    CHECK_EQ(bad_upper_map, 0);
    CHECK_EQ(tolower('Q'), 'q');
    CHECK_EQ(toupper('q'), 'Q');
    CHECK_EQ(tolower('7'), '7');
    CHECK_EQ(toupper(-1), -1);                            // EOF passes through
    CHECK_EQ(tolower(0xC9), 0xC9);                        // no accents in the "C" locale

    TEST_SECTION("isascii and toascii");
    CHECK(isascii(0) && isascii(127) && !isascii(128) && !isascii(-1));
    CHECK_EQ(toascii(0x141), 0x41);
    return test_summary();
}
