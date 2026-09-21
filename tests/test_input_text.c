// Typed text: with no window nothing is typed, so the queue is empty; and the UTF-8 encoder, whose answers do not
// depend on any device.
#include "ceres/test.h"
#include "ceres/input.h"
#include "ceres/keyboard.h"

static int encoded(unsigned int cp, unsigned char* out)
{
    char bytes[4];
    int n = utf8_encode(cp, bytes);
    for (int i = 0; i < n; i++)
        out[i] = (unsigned char)bytes[i];
    return n;
}

int main(void)
{
    TEST_SECTION("no text without a window");
    CHECK_EQ(input_text_ready(), 0);
    CHECK_EQ(kbd_text_ready(), 0);
    CHECK_EQ((int)input_text(), 0);
    CHECK_EQ((int)kbd_read_text(), 0);

    TEST_SECTION("utf8_encode");
    unsigned char b[4];
    CHECK_EQ(encoded('A', b), 1);
    CHECK_EQ(b[0], 'A');
    CHECK_EQ(encoded(0, b), 1);
    CHECK_EQ(encoded(0x7F, b), 1);
    CHECK_EQ(encoded(0xE9, b), 2);                      // e with an acute accent
    CHECK_EQ(b[0], 0xC3);
    CHECK_EQ(b[1], 0xA9);
    CHECK_EQ(encoded(0x7FF, b), 2);
    CHECK_EQ(b[0], 0xDF);
    CHECK_EQ(b[1], 0xBF);
    CHECK_EQ(encoded(0x800, b), 3);
    CHECK_EQ(b[0], 0xE0);
    CHECK_EQ(b[1], 0xA0);
    CHECK_EQ(b[2], 0x80);
    CHECK_EQ(encoded(0x20AC, b), 3);                    // the euro sign
    CHECK_EQ(b[0], 0xE2);
    CHECK_EQ(b[1], 0x82);
    CHECK_EQ(b[2], 0xAC);
    CHECK_EQ(encoded(0xFFFF, b), 3);
    CHECK_EQ(encoded(0x10000, b), 4);
    CHECK_EQ(b[0], 0xF0);
    CHECK_EQ(b[1], 0x90);
    CHECK_EQ(b[2], 0x80);
    CHECK_EQ(b[3], 0x80);
    CHECK_EQ(encoded(0x1F600, b), 4);                   // a grinning face
    CHECK_EQ(b[0], 0xF0);
    CHECK_EQ(b[1], 0x9F);
    CHECK_EQ(b[2], 0x98);
    CHECK_EQ(b[3], 0x80);
    CHECK_EQ(encoded(0x10FFFF, b), 4);
    CHECK_EQ(encoded(0x110000, b), 0);                  // past the last code point
    CHECK_EQ(encoded(0xD800, b), 0);                    // a surrogate is not a character
    CHECK_EQ(encoded(0xDFFF, b), 0);
    CHECK_EQ(encoded(0xE000, b), 3);                    // the first one after them

    return test_summary();
}
