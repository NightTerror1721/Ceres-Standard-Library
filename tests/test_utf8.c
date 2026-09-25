// UTF-8 and wide characters: ceres/utf8.h, the conversions of uchar.h, wchar.h and stdlib.h, printf's %lc and %ls
// and scanf's, and text with accents in the text framebuffer (whose frame reaches the terminal as UTF-8) and in
// the pixel font.
#include "ceres/test.h"
#include "ceres/utf8.h"
#include "ceres/textfb.h"
#include "ceres/font.h"
#include "ceres/gfx.h"
#include "uchar.h"
#include "wchar.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "errno.h"
#include "limits.h"

int main(void)
{
    TEST_SECTION("decoding and encoding");
    unsigned int cp = 0;
    CHECK_EQ(utf8_decode("A", 1, &cp), 1);
    CHECK_EQ(cp, 'A');
    CHECK_EQ(utf8_decode("\xC3\xB1", 2, &cp), 2);                   // ñ
    CHECK_EQ(cp, 0xF1u);
    CHECK_EQ(utf8_decode("\xE2\x82\xAC", 3, &cp), 3);               // €
    CHECK_EQ(cp, 0x20ACu);
    CHECK_EQ(utf8_decode("\xF0\x9F\x98\x80", 4, &cp), 4);           // U+1F600
    CHECK_EQ(cp, 0x1F600u);
    CHECK_EQ(utf8_decode("\xE2\x82\xAC", 2, &cp), -1);              // cut short
    CHECK_EQ(utf8_decode("\xC0\x80", 2, &cp), -1);                  // an overlong NUL
    CHECK_EQ(utf8_decode("\xE0\x80\x80", 3, &cp), -1);              // overlong
    CHECK_EQ(utf8_decode("\xED\xA0\x80", 3, &cp), -1);              // a surrogate
    CHECK_EQ(utf8_decode("\xF4\x90\x80\x80", 4, &cp), -1);          // past U+10FFFF
    CHECK_EQ(utf8_decode("\x80", 1, &cp), -1);                      // a continuation alone
    CHECK_EQ(utf8_decode("", 0, &cp), 0);
    char out[UTF8_MAX];
    CHECK_EQ(utf8_encode(0xE9, out), 2);
    CHECK(memcmp(out, "\xC3\xA9", 2) == 0);
    CHECK_EQ(utf8_encode(0x10FFFF, out), 4);
    CHECK(memcmp(out, "\xF4\x8F\xBF\xBF", 4) == 0);
    CHECK_EQ(utf8_encode(0xD800, out), 0);
    CHECK_EQ(utf8_encode(0x110000, out), 0);
    CHECK_EQ(utf8_width(0x7F), 1);
    CHECK_EQ(utf8_width(0x7FF), 2);
    CHECK_EQ(utf8_width(0xFFFF), 3);

    TEST_SECTION("walking a string");
    const char* p = "a\xC3\xB1o\x80!";                             // "año", a stray byte, "!"
    CHECK_EQ(utf8_next(&p), 'a');
    CHECK_EQ(utf8_next(&p), 0xF1u);
    CHECK_EQ(utf8_next(&p), 'o');
    CHECK_EQ(utf8_next(&p), UTF8_REPLACEMENT);
    CHECK_EQ(utf8_next(&p), '!');
    CHECK_EQ(utf8_next(&p), 0u);
    CHECK_EQ(utf8_next(&p), 0u);                                     // stays at the end
    CHECK_EQ((int)utf8_length("\xC2\xBFQu\xC3\xA9 a\xC3\xB1o?"), 9);   // ¿Qué año?
    CHECK_EQ((int)utf8_length("\xE2\x82"), 2);                       // cut short: two broken bytes
    CHECK_EQ(utf8_valid("\xC3\xA9t\xC3\xA9", 5), 1);
    CHECK_EQ(utf8_valid("\xC3\xA9t\xC3", 4), 0);
    CHECK_STR(utf8_offset("\xC3\xA9t\xC3\xA9", 2), "\xC3\xA9");
    CHECK_STR(utf8_offset("ab", 9), "");

    TEST_SECTION("to and from Latin-1");
    char latin[8];
    CHECK_EQ((int)utf8_to_latin1(latin, sizeof latin, "a\xC3\xB1o \xE2\x82\xAC", '?'), 5);
    CHECK_STR(latin, "a\xF1o ?");
    CHECK_EQ((int)utf8_to_latin1(latin, 3, "\xC3\xA9t\xC3\xA9", '?'), 3);   // cut short: the length it needs
    CHECK_STR(latin, "\xE9t");
    char back[8];
    CHECK_EQ((int)latin1_to_utf8(back, sizeof back, "\xE9t\xE9"), 5);
    CHECK_STR(back, "\xC3\xA9t\xC3\xA9");
    CHECK_EQ((int)latin1_to_utf8(back, 4, "\xE9t\xE9"), 5);          // the second é does not fit whole
    CHECK_STR(back, "\xC3\xA9t");

    TEST_SECTION("uchar.h");
    mbstate_t st;
    memset(&st, 0, sizeof st);
    char32_t c32 = 0;
    CHECK_EQ((int)mbrtoc32(&c32, "\xE2\x82\xAC!", 4, &st), 3);
    CHECK_EQ(c32, 0x20ACu);
    CHECK_EQ(mbrtoc32(&c32, "\xE2", 1, &st), (size_t)-2);            // the start of a character
    CHECK(!mbsinit(&st));
    CHECK_EQ((int)mbrtoc32(&c32, "\x82\xAC", 2, &st), 2);            // ...and the rest of it
    CHECK_EQ(c32, 0x20ACu);
    CHECK(mbsinit(&st));
    CHECK_EQ((int)mbrtoc32(&c32, "", 1, &st), 0);                    // the NUL
    errno = 0;
    CHECK_EQ(mbrtoc32(&c32, "\xFF", 1, &st), (size_t)-1);
    CHECK_EQ(errno, EILSEQ);
    char mb[MB_LEN_MAX];
    CHECK_EQ((int)c32rtomb(mb, 0xF1, &st), 2);
    CHECK(memcmp(mb, "\xC3\xB1", 2) == 0);
    CHECK_EQ(c32rtomb(mb, 0xDC00, &st), (size_t)-1);
    char16_t c16 = 0;
    CHECK_EQ((int)mbrtoc16(&c16, "\xF0\x9F\x98\x80", 4, &st), 4);    // U+1F600: a pair
    CHECK_EQ(c16, 0xD83Du);
    CHECK_EQ(mbrtoc16(&c16, "", 0, &st), (size_t)-3);                // the trail, reading nothing
    CHECK_EQ(c16, 0xDE00u);
    CHECK_EQ((int)c16rtomb(mb, 0xD83D, &st), 0);                     // a lead: kept
    CHECK_EQ((int)c16rtomb(mb, 0xDE00, &st), 4);
    CHECK(memcmp(mb, "\xF0\x9F\x98\x80", 4) == 0);
    CHECK_EQ(c16rtomb(mb, 0xDE00, &st), (size_t)-1);                 // a trail alone
    char8_t c8 = 0;
    CHECK_EQ((int)mbrtoc8(&c8, "\xC3\xB1", 2, &st), 2);
    CHECK_EQ(c8, 0xC3u);
    CHECK_EQ(mbrtoc8(&c8, "", 0, &st), (size_t)-3);
    CHECK_EQ(c8, 0xB1u);
    CHECK_EQ((int)c8rtomb(mb, 0xC3, &st), 0);                        // nothing until the character is whole
    CHECK_EQ((int)c8rtomb(mb, 0xB1, &st), 2);
    CHECK(memcmp(mb, "\xC3\xB1", 2) == 0);

    TEST_SECTION("wchar.h and stdlib.h");
    CHECK_EQ(MB_CUR_MAX, 4);
    CHECK_EQ(mblen("\xC3\xB1", 2), 2);
    CHECK_EQ(mblen("\xC3\xB1", 1), -1);                              // not a whole character in one byte
    CHECK_EQ(mblen("", 1), 0);
    CHECK_EQ(mblen(0, 0), 0);                                        // UTF-8 has no shift states
    wchar_t wc = 0;
    CHECK_EQ(mbtowc(&wc, "\xE2\x82\xAC", 3), 3);
    CHECK_EQ(wc, 0x20AC);
    CHECK_EQ(wctomb(mb, 0x20AC), 3);
    CHECK_EQ(wctomb(mb, -1), -1);
    wchar_t wide[8];
    CHECK_EQ((int)mbstowcs(wide, "a\xC3\xB1o", 8), 3);
    CHECK(wide[0] == 'a' && wide[1] == 0xF1 && wide[2] == 'o' && wide[3] == 0);
    CHECK_EQ((int)mbstowcs(0, "a\xC3\xB1o", 0), 3);                  // just the length
    CHECK_EQ(mbstowcs(wide, "a\xC3", 8), (size_t)-1);
    char narrow[8];
    CHECK_EQ((int)wcstombs(narrow, L"a\x00F1o", sizeof narrow), 4);
    CHECK_STR(narrow, "a\xC3\xB1o");
    CHECK_EQ((int)wcstombs(narrow, L"\x00F1\x00F1\x00F1", 5), 4);    // the third would not fit whole
    CHECK(memcmp(narrow, "\xC3\xB1\xC3\xB1", 4) == 0);
    CHECK_EQ((int)wcstombs(0, L"\x20AC!", 0), 4);
    const char* src = "\xC3\xA9x";
    memset(&st, 0, sizeof st);
    CHECK_EQ((int)mbsrtowcs(wide, &src, 1, &st), 1);                 // room for one: src moves past it
    CHECK_STR(src, "x");
    CHECK_EQ((int)mbsrtowcs(wide, &src, 8, &st), 1);
    CHECK(src == 0);                                                 // the terminator was converted
    CHECK_EQ(btowc('A'), (wint_t)'A');
    CHECK_EQ(btowc(0xC3), WEOF);
    CHECK_EQ(wctob(0xF1), EOF);
    CHECK_EQ((int)wcslen(L"a\x00F1o"), 3);
    wchar_t w1[16];
    wcscpy(w1, L"pa");
    wcscat(w1, L"\x00F1o");
    CHECK(wcscmp(w1, L"pa\x00F1o") == 0);
    CHECK(wcscmp(L"a", L"b") < 0);
    CHECK(wcsncmp(L"abc", L"abd", 2) == 0);
    CHECK(wcschr(w1, 0xF1) == w1 + 2);
    CHECK(wcsrchr(L"abca", 'a') != 0);
    CHECK(wcsstr(w1, L"\x00F1o") == w1 + 2);
    wmemset(wide, 'z', 3);
    CHECK(wmemcmp(wide, L"zzz", 3) == 0);
    wmemmove(wide + 1, wide, 2);
    CHECK(wmemchr(wide, 'z', 3) == wide);
    wcscpy(w1, L"ab");
    wcsncat(w1, L"cdef", 2);
    CHECK(wcscmp(w1, L"abcd") == 0);
    CHECK_EQ((int)wcsspn(L"aab!", L"ab"), 3);
    CHECK_EQ((int)wcscspn(L"xy,z", L",;"), 2);
    CHECK(wcspbrk(L"xy;z", L",;") != 0 && *wcspbrk(L"xy;z", L",;") == ';');
    wchar_t tokens[16];
    wcscpy(tokens, L" uno, dos ");
    wchar_t* save = 0;
    wchar_t* first = wcstok(tokens, L" ,", &save);
    wchar_t* second = wcstok(0, L" ,", &save);
    CHECK(first != 0 && wcscmp(first, L"uno") == 0 && second != 0 && wcscmp(second, L"dos") == 0);
    CHECK(wcstok(0, L" ,", &save) == 0);
    CHECK(wcscoll(L"a", L"b") < 0);
    CHECK_EQ((int)wcsxfrm(w1, L"xyz", 16), 3);

    TEST_SECTION("printf and scanf");
    char line[32];
    snprintf(line, sizeof line, "[%lc|%ls|%5ls|%.3ls]", (wint_t)0xF1, L"a\x00F1o", L"\x00E9", L"\x00F1\x00F1");
    CHECK_STR(line, "[\xC3\xB1|a\xC3\xB1o|   \xC3\xA9|\xC3\xB1]");   // widths and precisions count bytes
    snprintf(line, sizeof line, "%lc", (wint_t)0xD800);
    CHECK_STR(line, "\xEF\xBF\xBD");                                 // not a character: U+FFFD
    wchar_t got[8];
    wchar_t one = 0;
    CHECK_EQ(sscanf("\xC3\xB1" "and\xC3\xBA rest", "%lc%ls", &one, got), 2);
    CHECK_EQ(one, 0xF1);
    CHECK(wcscmp(got, L"and\x00FA") == 0);
    CHECK_EQ(sscanf("abc\xC3\xA9x", "%2ls", got), 1);                // the width counts characters
    CHECK(wcscmp(got, L"ab") == 0);
    CHECK_EQ(sscanf("abc,d", "%l[a-z]", got), 1);
    CHECK(wcscmp(got, L"abc") == 0);
    CHECK_EQ(sscanf("a\xC3", "%ls", got), 0);                        // cut short: not UTF-8
    printf("printed: \xC2\xBFQu\xC3\xA9 a\xC3\xB1o? %ls\n", L"\x00A1S\x00ED!");

    TEST_SECTION("the text framebuffer");
    CHECK_EQ(fb_init(12, 2), 0);
    fb_text(0, 0, "\xC2\xBFQu\xC3\xA9 a\xC3\xB1o?");                 // nine characters, nine cells
    CHECK_EQ((unsigned char)fb_get(0, 0), 0xBFu);
    CHECK_EQ((unsigned char)fb_get(3, 0), 0xE9u);
    CHECK_EQ(fb_get(8, 0), '?');
    CHECK_EQ(fb_get(9, 0), ' ');
    CHECK_EQ(fb_text_width("\xC3\xA9t\xC3\xA9\nab"), 3);
    CHECK_EQ(fb_text_n(0, 1, "\xE2\x82\xAC" "uro\xC3\xA9xtra", 5), 5);   // € is past Latin-1: '?'
    CHECK_EQ(fb_get(0, 1), '?');
    fb_put_char(11, 1, 0xDF);                                        // ß
    fb_present();                                                    // the terminal gets it in UTF-8
    fb_shutdown();

    TEST_SECTION("the pixel font");
    CHECK(font8x8[0xE9][0] != 0);                                    // é has its accent on the top row
    CHECK_EQ(font8x8['e'][0], 0);
    CHECK(memcmp(font8x8[0xE9] + 2, font8x8['e'] + 2, 5) == 0);      // ...over the body of an e
    CHECK_EQ(font_text_width("a\xC3\xB1o", 1), 3 * FONT_W);
    static unsigned int pixels[3 * FONT_W * FONT_H];
    struct gfx_surface surface = { pixels, 3 * FONT_W, FONT_H };
    font_text(&surface, 0, 0, "\xC3\xB1", 0xFFFFFFu, 1);     // ñ: the tilde lights its top row
    int lit = 0;
    for (int x = 0; x < FONT_W; x++)
        lit += pixels[x] != 0;
    CHECK(lit > 0);
    font_text(&surface, FONT_W, 0, "\xE2\x82\xAC", 0xFFFFFFu, 1);   // past Latin-1: the box of 127
    CHECK(pixels[FONT_W + 1] != 0 && pixels[FONT_W + 6] != 0);
    return test_summary();
}
