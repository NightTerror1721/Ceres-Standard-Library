// The input devices and the per-frame input state. A headless run has no window, so the devices report
// nothing: what is checked there is that they answer sanely and that the arithmetic (dead zones, edges,
// key <-> character) is right. The frame logic is driven with synthetic events; the terminal fallback reads
// the characters in tests/expected/test_input.stdin.
#include "ceres/test.h"
#include "ceres/keyboard.h"
#include "ceres/mouse.h"
#include "ceres/gamepad.h"
#include "ceres/input.h"
#include "ceres/terminal.h"

static void wait_for_terminal_bytes(int count)
{
    // the host hands the input over at its own pace: give it a generous number of iterations
    for (int spin = 0; spin < 3000000 && term_bytes_available() < count; spin++)
    {
    }
}

int main(void)
{
    TEST_SECTION("devices with nothing attached to a window");
    CHECK_EQ(kbd_event_ready(), 0);
    CHECK_EQ((int)kbd_read_event(), 0);
    unsigned int events[KBD_QUEUE_SIZE];
    CHECK_EQ(kbd_read_block(events, KBD_QUEUE_SIZE), 0);
    CHECK_EQ(kbd_read_block(events, 1000), 0);                   // more than the queue holds is clamped
    CHECK_EQ(kbd_read_block(events, 0), 0);
    CHECK_EQ(kbd_read_block(0, 8), 0);
    CHECK_EQ(kbd_flush(), 0);
    CHECK_EQ(mouse_moved(), 0);
    CHECK_EQ(mouse_dx() + mouse_dy() + mouse_wheel() + mouse_x() + mouse_y() + mouse_buttons(), 0);
    CHECK_EQ(gp_changed(), 0);
    CHECK_EQ((int)gp_buttons(), 0);
    CHECK_EQ(gp_left_x() + gp_left_y() + gp_right_x() + gp_right_y() + gp_left_trigger() + gp_right_trigger(), 0);
    struct mouse_state ms;
    mouse_poll(&ms);
    CHECK(ms.x == 0 && ms.y == 0 && ms.dx == 0 && ms.dy == 0 && ms.wheel == 0);
    CHECK(ms.buttons == 0 && ms.pressed == 0 && ms.released == 0);
    struct gp_state gs;
    gp_poll(&gs);
    CHECK(gs.buttons == 0 && gs.pressed == 0 && gs.released == 0);
    CHECK(gs.lx == 0 && gs.ly == 0 && gs.rx == 0 && gs.ry == 0 && gs.lt == 0 && gs.rt == 0);

    TEST_SECTION("keyboard event fields");
    unsigned int down_a = KBD_EVENT_PRESSED | KEY_A;
    unsigned int up_a = KEY_A;
    CHECK(kbd_is_pressed(down_a) && !kbd_is_pressed(up_a));
    CHECK_EQ(kbd_keycode(down_a), KEY_A);
    CHECK_EQ(kbd_keycode(up_a), KEY_A);
    CHECK_EQ(kbd_keycode(KBD_EVENT_PRESSED | 0x7FFFFFFFu), 0x7FFFFFFF);
    CHECK(KEY_A == 4 && KEY_ENTER == 40 && KEY_UP == 82 && KEY_LSHIFT == 225);   // USB HID, as SDL3 reports

    TEST_SECTION("gamepad dead zone");
    CHECK_EQ(gp_deadzone(0, 4096), 0);
    CHECK_EQ(gp_deadzone(4096, 4096), 0);                        // inside, and on the edge of, the zone
    CHECK_EQ(gp_deadzone(-4096, 4096), 0);
    CHECK_EQ(gp_deadzone(4097, 4096), 1);                        // just outside reads ~0, not a jump to 4096
    CHECK_EQ(gp_deadzone(-4097, 4096), -1);
    CHECK_EQ(gp_deadzone(16384, 4096), 14043);
    CHECK_EQ(gp_deadzone(32767, 4096), 32767);                   // the full range is still reached
    CHECK_EQ(gp_deadzone(-32768, 4096), -32767);
    CHECK_EQ(gp_deadzone(100, 0), 100);                          // no zone: unchanged
    CHECK_EQ(gp_deadzone(100, -5), 100);                         // a negative zone is no zone
    CHECK_EQ(gp_deadzone(32000, 32767), 0);                      // a zone as wide as the range swallows everything
    CHECK_EQ(gp_deadzone(20000, 20000), 0);
    int rising = 1;
    for (int v = 4200; v < 32767; v += 500)
        if (gp_deadzone(v, 4096) <= gp_deadzone(v - 500, 4096)) rising = 0;
    CHECK(rising);                                               // monotonic

    TEST_SECTION("gamepad axis to float");
    CHECK(gp_axis_f(0) == 0.0f);
    CHECK(gp_axis_f(32767) == 1.0f);
    CHECK(gp_axis_f(-32768) == -1.0f);                           // one step past -1 is clamped
    CHECK(gp_axis_f(-32767) == -1.0f);
    CHECK_NEAR(gp_axis_f(16384), 0.5f, 0.0001f);
    CHECK_NEAR(gp_axis_f(-8192), -0.25f, 0.0001f);
    CHECK(gp_axis_f(100000) == 1.0f);

    TEST_SECTION("keys: pressed, held, released");
    input_init();
    CHECK(!key_down(KEY_A) && !key_pressed(KEY_A) && !key_released(KEY_A));
    input_begin_frame();
    input_key_event(KEY_A, 1);                                   // frame 1: A goes down
    CHECK(key_down(KEY_A) && key_pressed(KEY_A) && !key_released(KEY_A));
    CHECK_EQ(key_any_pressed(), KEY_A);
    input_begin_frame();                                         // frame 2: still held
    CHECK(key_down(KEY_A) && !key_pressed(KEY_A) && !key_released(KEY_A));
    CHECK_EQ(key_any_pressed(), 0);
    input_key_event(KEY_A, 1);                                   // a repeat of a held key is not a new press
    CHECK(key_down(KEY_A) && !key_pressed(KEY_A));
    input_begin_frame();                                         // frame 3: A goes up
    input_key_event(KEY_A, 0);
    CHECK(!key_down(KEY_A) && !key_pressed(KEY_A) && key_released(KEY_A));
    input_begin_frame();                                         // frame 4: quiet
    CHECK(!key_down(KEY_A) && !key_pressed(KEY_A) && !key_released(KEY_A));
    input_key_event(KEY_A, 0);                                   // releasing a key that was not down: nothing
    CHECK(!key_released(KEY_A));

    TEST_SECTION("keys: a tap inside one frame");
    input_begin_frame();
    input_key_event(KEY_B, 1);
    input_key_event(KEY_B, 0);                                   // down and up before the next update
    CHECK(key_pressed(KEY_B));                                   // the tap is not lost ...
    CHECK(key_released(KEY_B));
    CHECK(!key_down(KEY_B));                                     // ... though it is no longer held
    input_begin_frame();
    CHECK(!key_pressed(KEY_B) && !key_released(KEY_B));

    TEST_SECTION("keys: several at once and the edges");
    input_begin_frame();
    input_key_event(KEY_LEFT, 1);
    input_key_event(KEY_SPACE, 1);
    input_key_event(KEY_LSHIFT, 1);
    CHECK(key_down(KEY_LEFT) && key_down(KEY_SPACE) && key_down(KEY_LSHIFT));
    CHECK(key_any_pressed() == KEY_SPACE);                       // the lowest scancode that went down: SPACE (44) < LEFT (80)
    input_begin_frame();
    input_key_event(KEY_SPACE, 0);
    CHECK(key_released(KEY_SPACE) && key_down(KEY_LEFT) && key_down(KEY_LSHIFT));
    CHECK(!key_pressed(KEY_LEFT));

    TEST_SECTION("keys: scancodes out of range");
    input_begin_frame();
    input_key_event(0, 1);
    input_key_event(-1, 1);
    input_key_event(256, 1);
    input_key_event(100000, 1);
    CHECK(!key_down(0) && !key_down(-1) && !key_down(256) && !key_down(100000));
    CHECK(!key_pressed(256) && !key_released(-5));
    input_key_event(255, 1);
    CHECK(key_down(255));                                        // the last valid one
    input_init();
    CHECK(!key_down(255) && !key_down(KEY_LEFT) && !key_pressed(KEY_A));   // init forgets it all

    TEST_SECTION("mouse and pad state without a device");
    input_update();
    CHECK(mouse_pos_x() == 0 && mouse_pos_y() == 0 && mouse_move_x() == 0 && mouse_move_y() == 0 && mouse_scroll() == 0);
    CHECK(!mouse_btn_down(MOUSE_BTN_LEFT | MOUSE_BTN_RIGHT | MOUSE_BTN_MIDDLE));
    CHECK(!mouse_btn_pressed(MOUSE_BTN_LEFT) && !mouse_btn_released(MOUSE_BTN_LEFT));
    CHECK(!pad_down(GP_BTN_SOUTH) && !pad_pressed(GP_BTN_SOUTH) && !pad_released(GP_BTN_SOUTH));
    CHECK(!pad_down(0));                                         // an empty mask is never "held"
    for (int axis = 0; axis <= PAD_AXIS_RIGHT_TRIGGER; axis++)
        CHECK(pad_axis(axis) == 0.0f);
    CHECK(pad_axis(99) == 0.0f);

    TEST_SECTION("key to character");
    CHECK_EQ(key_to_ascii(KEY_A, 0), 'a');
    CHECK_EQ(key_to_ascii(KEY_A, 1), 'A');
    CHECK_EQ(key_to_ascii(KEY_Z, 0), 'z');
    CHECK_EQ(key_to_ascii(KEY_Z, 1), 'Z');
    CHECK_EQ(key_to_ascii(KEY_1, 0), '1');
    CHECK_EQ(key_to_ascii(KEY_9, 0), '9');
    CHECK_EQ(key_to_ascii(KEY_0, 0), '0');
    CHECK_EQ(key_to_ascii(KEY_1, 1), '!');
    CHECK_EQ(key_to_ascii(KEY_2, 1), '@');
    CHECK_EQ(key_to_ascii(KEY_9, 1), '(');
    CHECK_EQ(key_to_ascii(KEY_0, 1), ')');
    CHECK_EQ(key_to_ascii(KEY_ENTER, 0), '\n');
    CHECK_EQ(key_to_ascii(KEY_SPACE, 0), ' ');
    CHECK_EQ(key_to_ascii(KEY_TAB, 0), '\t');
    CHECK_EQ(key_to_ascii(KEY_ESCAPE, 0), 27);
    CHECK_EQ(key_to_ascii(KEY_BACKSPACE, 0), 8);
    CHECK_EQ(key_to_ascii(KEY_MINUS, 0), '-');
    CHECK_EQ(key_to_ascii(KEY_MINUS, 1), '_');
    CHECK_EQ(key_to_ascii(KEY_EQUALS, 0), '=');
    CHECK_EQ(key_to_ascii(KEY_EQUALS, 1), '+');
    CHECK_EQ(key_to_ascii(KEY_LEFTBRACKET, 0), '[');
    CHECK_EQ(key_to_ascii(KEY_RIGHTBRACKET, 1), '}');
    CHECK_EQ(key_to_ascii(KEY_BACKSLASH, 0), '\\');
    CHECK_EQ(key_to_ascii(KEY_BACKSLASH, 1), '|');
    CHECK_EQ(key_to_ascii(KEY_SEMICOLON, 0), ';');
    CHECK_EQ(key_to_ascii(KEY_SEMICOLON, 1), ':');
    CHECK_EQ(key_to_ascii(KEY_APOSTROPHE, 1), '"');
    CHECK_EQ(key_to_ascii(KEY_GRAVE, 1), '~');
    CHECK_EQ(key_to_ascii(KEY_COMMA, 1), '<');
    CHECK_EQ(key_to_ascii(KEY_PERIOD, 0), '.');
    CHECK_EQ(key_to_ascii(KEY_SLASH, 1), '?');
    CHECK_EQ(key_to_ascii(KEY_KP_7, 0), '7');
    CHECK_EQ(key_to_ascii(KEY_KP_0, 0), '0');
    CHECK_EQ(key_to_ascii(KEY_KP_PLUS, 0), '+');
    CHECK_EQ(key_to_ascii(KEY_KP_ENTER, 0), '\n');
    CHECK_EQ(key_to_ascii(KEY_UP, 0), 0);                        // no character for an arrow, F1 or a modifier
    CHECK_EQ(key_to_ascii(KEY_F1, 0), 0);
    CHECK_EQ(key_to_ascii(KEY_LSHIFT, 1), 0);
    CHECK_EQ(key_to_ascii(0, 0), 0);
    CHECK_EQ(key_to_ascii(300, 0), 0);
    int letters_ok = 1;
    for (int i = 0; i < 26; i++)                                 // every letter key types its own letter
        if (key_to_ascii(KEY_A + i, 0) != 'a' + i || key_to_ascii(KEY_A + i, 1) != 'A' + i) letters_ok = 0;
    CHECK(letters_ok);

    TEST_SECTION("terminal fallback");
    input_init();
    wait_for_terminal_bytes(6);                                  // the stdin file is "wD 1\nq" = 6 bytes
    input_update();                                              // fallback is off: the terminal is left alone
    CHECK(!key_down(KEY_W));
    CHECK_EQ(term_bytes_available(), 6);
    input_terminal_fallback(1);
    input_update();
    CHECK(key_pressed(KEY_W) && key_down(KEY_W));                // 'w'
    CHECK(key_pressed(KEY_D) && key_down(KEY_D));                // 'D' is shift + d
    CHECK(key_down(KEY_LSHIFT));
    CHECK(key_pressed(KEY_SPACE));
    CHECK(key_pressed(KEY_1));
    CHECK(key_pressed(KEY_ENTER));
    CHECK(key_pressed(KEY_Q));
    CHECK(!key_down(KEY_A) && !key_pressed(KEY_S));
    CHECK_EQ(term_bytes_available(), 0);                         // it took everything that was waiting
    input_update();                                              // the next frame: a terminal keystroke has ended
    CHECK(!key_down(KEY_W) && !key_pressed(KEY_W) && key_released(KEY_W));
    CHECK(!key_down(KEY_LSHIFT) && key_released(KEY_LSHIFT));
    input_update();
    CHECK(!key_released(KEY_W));
    input_terminal_fallback(0);
    return test_summary();
}
