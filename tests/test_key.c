// Keystrokes (ceres/key.h). Here the input is a file, so the host cannot hand over raw keys: they are decoded
// from the bytes a terminal sends, which test_key.stdin spells out one after another. The keyboard device's own
// words are normalised by the same function that turns them into keystrokes on a console and in a window.
#include "ceres/test.h"
#include "ceres/key.h"

// CHECK_EQ evaluates its arguments a second time to print them, and reading a key takes it: read once, then compare.
#define KEY_IS(expected) do { int key_ = key_wait(); CHECK_EQ(key_, expected); } while (0)

int main(void)
{
    TEST_SECTION("a file cannot be given raw keys");
    CHECK_EQ(term_set_raw(1), 0);                     // asked, and not granted
    CHECK_EQ(key_start(), 0);                         // so the keys are read from the bytes
    CHECK_EQ(key_start(), 0);                         // and calls nest
    key_stop();
    key_stop();
    KEY_IS('a');                                      // the first byte of the file

    TEST_SECTION("the keyboard register, as keystrokes");
    CHECK_EQ(key_from_keystroke('q'), 'q');
    CHECK_EQ(key_from_keystroke(0xE9), 0xE9);         // a character is its code point
    CHECK_EQ(key_from_keystroke(0x20AC), 0x20AC);
    CHECK_EQ(key_from_keystroke(0x1F600), 0x1F600);
    CHECK_EQ(key_from_keystroke(KBD_KEY_NAMED | KEY_ENTER), KEYC_ENTER);
    CHECK_EQ(key_from_keystroke(KBD_KEY_NAMED | KEY_KP_ENTER), KEYC_ENTER);   // the keypad's Enter is Enter
    CHECK_EQ(key_from_keystroke(KBD_KEY_NAMED | KEY_UP), KEYC_UP);
    CHECK_EQ(key_from_keystroke(KBD_KEY_NAMED | KEY_F1), KEYC_F(1));
    CHECK_EQ(key_from_keystroke(KBD_KEY_NAMED | KEY_F12), KEYC_F(12));
    CHECK(KEYC_UP > 0x10FFFF);                        // above every code point: a key and a character never meet
    CHECK(KEYC_ENTER != KEYC_ESC);

    TEST_SECTION("characters");
    KEY_IS(0xE9);
    KEY_IS(0x20AC);

    TEST_SECTION("Enter, however it is spelled");
    KEY_IS(KEYC_ENTER);                 // \r\n is one Enter
    KEY_IS(KEYC_ENTER);                 // \n
    KEY_IS(KEYC_ENTER);                 // \r on its own
    KEY_IS('z');                        // and what followed it

    TEST_SECTION("Backspace and Tab");
    KEY_IS(KEYC_BACKSPACE);             // 127
    KEY_IS(KEYC_BACKSPACE);             // 8
    KEY_IS(KEYC_TAB);

    TEST_SECTION("the arrows");
    KEY_IS(KEYC_UP);
    KEY_IS(KEYC_DOWN);
    KEY_IS(KEYC_RIGHT);
    KEY_IS(KEYC_LEFT);
    KEY_IS(KEYC_UP);                    // ESC O A

    TEST_SECTION("the editing keys");
    KEY_IS(KEYC_HOME);                  // ESC [ H
    KEY_IS(KEYC_END);                   // ESC [ F
    KEY_IS(KEYC_HOME);                  // ESC [ 1 ~
    KEY_IS(KEYC_INSERT);
    KEY_IS(KEYC_DELETE);
    KEY_IS(KEYC_END);                   // ESC [ 4 ~
    KEY_IS(KEYC_PAGEUP);
    KEY_IS(KEYC_PAGEDOWN);

    TEST_SECTION("a modifier does not hide the key");
    KEY_IS(KEYC_UP);                    // ESC [ 1 ; 5 A
    KEY_IS(KEYC_DELETE);                // ESC [ 3 ; 2 ~

    TEST_SECTION("function keys");
    KEY_IS(KEYC_F(1));
    KEY_IS(KEYC_F(5));
    KEY_IS(KEYC_F(12));

    TEST_SECTION("what is not a key is dropped");
    KEY_IS('x');                        // after ESC [ 9 9 ~
    KEY_IS(KEYC_ESC);                   // an Escape that something ordinary follows ...
    KEY_IS('y');                        // ... and that
    KEY_IS('w');                        // after a control character
    KEY_IS('b');                        // after half a character

    TEST_SECTION("the end");
    KEY_IS(KEYC_ESC);                                 // an Escape with nothing after it, not the start of a sequence
    KEY_IS(KEYC_NONE);                                // nothing more, and none coming: waiting does not hang
    int none = key_get();
    CHECK_EQ(none, KEYC_NONE);                        // and the non-blocking read says so too
    return test_summary();
}
