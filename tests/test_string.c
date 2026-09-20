#include "ceres/test.h"
#include "string.h"
#include "strings.h"
#include "ceres/heap.h"

int main(void)
{
    char a[48];
    char b[48];

    TEST_SECTION("memcpy");
    for (int i = 0; i < 40; i++) a[i] = (char)(i + 1);
    for (int off = 0; off < 4; off++)                    // every mix of alignments
        for (int src = 0; src < 4; src++)
            for (int len = 0; len <= 33; len += 3)
            {
                for (int i = 0; i < 48; i++) b[i] = 0;
                memcpy(b + off, a + src, len);
                int ok = 1;
                for (int i = 0; i < len; i++) if (b[off + i] != a[src + i]) ok = 0;
                for (int i = 0; i < off; i++) if (b[i] != 0) ok = 0;
                if (b[off + len] != 0) ok = 0;           // nothing written past the end
                CHECK(ok);
            }
    CHECK(memcpy(b, a, 8) == b);                         // returns dst

    TEST_SECTION("memmove");
    for (int i = 0; i < 40; i++) a[i] = (char)(i + 1);
    memmove(a + 2, a, 30);                               // overlapping, dst above src
    int ok = 1;
    for (int i = 0; i < 30; i++) if (a[2 + i] != (char)(i + 1)) ok = 0;
    CHECK(ok);
    for (int i = 0; i < 40; i++) a[i] = (char)(i + 1);
    memmove(a, a + 2, 30);                               // overlapping, dst below src
    ok = 1;
    for (int i = 0; i < 30; i++) if (a[i] != (char)(i + 3)) ok = 0;
    CHECK(ok);

    TEST_SECTION("memset / memcmp / memchr");
    memset(b, 0xAB, 37);
    ok = 1;
    for (int i = 0; i < 37; i++) if ((b[i] & 255) != 0xAB) ok = 0;
    CHECK(ok);
    CHECK(b[37] != (char)0xAB);
    memset(b + 1, 0, 0);                                 // n == 0 touches nothing
    CHECK_EQ(memcmp("abc", "abd", 3), -1);
    CHECK_EQ(memcmp("abc", "abc", 3), 0);
    CHECK(memcmp("\xC8", "a", 1) > 0);                   // bytes compare as unsigned
    CHECK_EQ(memcmp("x", "y", 0), 0);
    const char* text = "find the needle";
    CHECK(memchr(text, 'n', 15) == text + 2);
    CHECK(memchr(text, 'z', 15) == 0);
    CHECK(memrchr(text, 'e', 15) == text + 14);
    CHECK(memmem(text, 15, "needle", 6) == text + 9);
    unsigned int words[4];
    memset32(words, 0xDEADBEEFu, 4);
    CHECK(words[0] == 0xDEADBEEFu && words[3] == 0xDEADBEEFu);

    TEST_SECTION("length, copy, concatenate");
    CHECK_EQ((int)strlen(""), 0);
    CHECK_EQ((int)strlen("hello"), 5);
    CHECK_EQ((int)strnlen("hello", 3), 3);
    strcpy(a, "hello");
    strcat(a, " world");
    CHECK_STR(a, "hello world");
    strncpy(b, "abc", 8);                                // pads the rest with NULs
    CHECK_EQ(b[3], 0);
    CHECK_EQ(b[7], 0);
    strncat(a, "!!!!", 2);
    CHECK_STR(a, "hello world!!");
    CHECK_EQ((int)strlcpy(b, "truncate me", 5), 11);     // returns the length it wanted
    CHECK_STR(b, "trun");
    strcpy(a, "ab");
    CHECK_EQ((int)strlcat(a, "cdef", 5), 6);
    CHECK_STR(a, "abcd");

    TEST_SECTION("compare");
    CHECK(strcmp("abc", "abd") < 0);
    CHECK(strcmp("b", "a") > 0);
    CHECK_EQ(strcmp("same", "same"), 0);
    CHECK(strcmp("", "a") < 0);
    CHECK(strcmp("\xC8", "a") > 0);                      // unsigned, as the standard requires
    CHECK_EQ(strncmp("abcX", "abcY", 3), 0);
    CHECK(strncmp("abcX", "abcY", 4) < 0);
    CHECK_EQ(strcasecmp("HeLLo", "hello"), 0);
    CHECK(strncasecmp("abcd", "ABCE", 3) == 0);
    CHECK(strcasecmp("a", "B") < 0);

    TEST_SECTION("search");
    const char* h = "hello";
    CHECK_EQ((int)(strchr(h, 'l') - h), 2);
    CHECK_EQ((int)(strrchr(h, 'l') - h), 3);
    CHECK(strchr(h, 'z') == 0);
    CHECK(strchr(h, 0) == h + 5);                        // the terminator is part of the string
    const char* hs = "find the needle";
    CHECK_EQ((int)(strstr(hs, "needle") - hs), 9);
    CHECK(strstr("abc", "abcd") == 0);
    CHECK(strstr(hs, "") == hs);
    CHECK_EQ((int)strspn("aabbcx", "abc"), 5);
    CHECK_EQ((int)strcspn("hello, world", ", "), 5);
    CHECK(strpbrk(h, "xyl") == h + 2);
    CHECK(strpbrk(h, "xyz") == 0);

    TEST_SECTION("tokens");
    strcpy(a, "a,b;;c");
    char* tok = strtok(a, ",;");
    CHECK_STR(tok, "a");
    tok = strtok(0, ",;");
    CHECK_STR(tok, "b");
    tok = strtok(0, ",;");
    CHECK_STR(tok, "c");
    CHECK(strtok(0, ",;") == 0);
    strcpy(a, "x y");
    char* save = 0;
    CHECK_STR(strtok_r(a, " ", &save), "x");
    CHECK_STR(strtok_r(0, " ", &save), "y");
    strcpy(a, "one:two");
    char* cursor = a;
    CHECK_STR(strsep(&cursor, ":"), "one");
    CHECK_STR(cursor, "two");

    TEST_SECTION("case and reverse");
    strcpy(a, "Hello");
    strupr(a);
    CHECK_STR(a, "HELLO");
    strlwr(a);
    CHECK_STR(a, "hello");
    strrev(a);
    CHECK_STR(a, "olleh");

    TEST_SECTION("duplicates and errors");
    char* dup = strdup("copy");
    CHECK_STR(dup, "copy");
    free(dup);
    char* nd = strndup("truncated", 5);
    CHECK_STR(nd, "trunc");
    free(nd);
    CHECK_STR(strerror(2), "No such file or directory");
    CHECK_STR(strerror(0), "Success");
    CHECK_EQ((int)heap_used(), 0);
    return test_summary();
}
