#include "ceres/key.h"
#include "ceres/terminal.h"
#include "ceres/timer.h"

// 1 once the host has agreed to deliver keys on the keyboard device; 0 while they are read from the terminal.
static int from_keyboard = 0;

// How many callers have asked for keys as they are pressed. The console is switched at the first and put back
// at the last, so a menu can be run from inside a program that is reading keys itself.
static int users = 0;

// A byte read ahead: an Escape followed by something that is not a sequence gives the Escape now and this byte
// on the next call. -1 when there is none.
static int held_byte = -1;

// Not a keystroke: a sequence that means nothing to us, dropped.
#define IGNORED (-2)

int key_start(void)
{
    if (users++ == 0)
        from_keyboard = (term_set_raw(1) & TERM_MODE_KEYS) != 0;
    return from_keyboard;
}

void key_stop(void)
{
    if (users > 0 && --users == 0)
    {
        term_set_raw(0);
        from_keyboard = 0;
    }
}

int key_from_keystroke(unsigned int keystroke)
{
    if (keystroke & KBD_KEY_NAMED)
    {
        int scancode = (int)(keystroke & 0xFFFFu);
        if (scancode == KEY_KP_ENTER)
            scancode = KEY_ENTER;
        return KEYC(scancode);
    }
    return (int)keystroke;
}

// ---- from the terminal's bytes -------------------------------------------------------------------------

// The next byte if there is one now, else -1.
static int next_byte(void)
{
    if (held_byte >= 0)
    {
        int b = held_byte;
        held_byte = -1;
        return b;
    }
    if (!term_read_ready())
        return -1;
    return term_read_char(TERM_READ_NON_BLOCKING) & 0xFF;
}

// How long to wait for the rest of a sequence, in milliseconds. The bytes of a file or a pipe reach the terminal
// a few at a time, from another thread of the host, so a program that reads faster than they arrive sees the
// start of a sequence before its end. This is also how long a lone Escape takes to be told from the start of one.
#define FOLLOW_MS 30

// The byte after one that promised more, waiting briefly for it (see FOLLOW_MS); -1 if none comes, or if the
// input has ended and none ever will.
static int following_byte(void)
{
    if (held_byte >= 0 || term_read_ready())
        return next_byte();
    if (term_eof())
        return -1;
    uint64_t deadline = timer_nanos64() + (uint64_t)FOLLOW_MS * 1000000u;
    for (;;)
    {
        if (term_read_ready())
            return next_byte();
        if (term_eof())
            return -1;
        if (timer_halt_until_ns(deadline))      // sleeps until a byte comes or the time is up
            break;
    }
    return term_read_ready() ? next_byte() : -1;
}

// The key an ESC [ ... sequence stands for, its final byte and first number known.
static int csi_key(int final, int number)
{
    switch (final)
    {
        case 'A': return KEYC_UP;
        case 'B': return KEYC_DOWN;
        case 'C': return KEYC_RIGHT;
        case 'D': return KEYC_LEFT;
        case 'H': return KEYC_HOME;
        case 'F': return KEYC_END;
        case 'P': return KEYC_F(1);
        case 'Q': return KEYC_F(2);
        case 'R': return KEYC_F(3);
        case 'S': return KEYC_F(4);
        case '~':
            switch (number)
            {
                case 1: case 7: return KEYC_HOME;
                case 2: return KEYC_INSERT;
                case 3: return KEYC_DELETE;
                case 4: case 8: return KEYC_END;
                case 5: return KEYC_PAGEUP;
                case 6: return KEYC_PAGEDOWN;
                case 11: case 12: case 13: case 14: return KEYC_F(1 + number - 11);
                case 15: return KEYC_F(5);
                case 17: case 18: case 19: case 20: case 21: return KEYC_F(6 + number - 17);
                case 23: case 24: return KEYC_F(11 + number - 23);
            }
    }
    return IGNORED;
}

// After an ESC that something followed. A sequence that has not fully arrived is dropped, as a slow keyboard
// never splits one; an Escape followed by an ordinary byte is the key, and the byte is kept for the next call.
static int decode_escape(void)
{
    int second = following_byte();
    if (second < 0)
        return KEYC_ESC;                              // nothing follows: the Escape key
    if (second == 'O')
    {
        int final = following_byte();
        return final < 0 ? IGNORED : csi_key(final, 0);
    }
    if (second != '[')
    {
        held_byte = second;
        return KEYC_ESC;
    }
    int number = 0;
    int have = 0;
    int first = 0;
    for (;;)
    {
        int b = following_byte();
        if (b < 0)
            return IGNORED;
        if (b >= '0' && b <= '9')
        {
            number = number * 10 + (b - '0');
            have = 1;
        }
        else if (b == ';')
        {
            if (!first)
                first = have ? number : 1;            // the modifier after the semicolon is not reported
            number = 0;
            have = 0;
        }
        else if (b >= 0x40 && b <= 0x7E)
            return csi_key(b, first ? first : number);
        else
            return IGNORED;
    }
}

// One byte just read, and what follows it if it starts something longer.
static int decode_byte(int c)
{
    if (c == 27)
        return decode_escape();
    if (c == '\r')
    {
        int follower = following_byte();
        if (follower != '\n' && follower >= 0)
            held_byte = follower;                     // \r alone is Enter; \r\n is one Enter, not two
        return KEYC_ENTER;
    }
    if (c == '\n')
        return KEYC_ENTER;
    if (c == 8 || c == 127)
        return KEYC_BACKSPACE;
    if (c == '\t')
        return KEYC_TAB;
    if (c < 32)
        return IGNORED;                               // another control character types nothing
    if (c < 0x80)
        return c;

    int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : -1;
    if (extra < 0)
        return IGNORED;                               // a stray continuation byte
    int point = c & (extra == 3 ? 0x07 : extra == 2 ? 0x0F : 0x1F);
    for (int i = 0; i < extra; i++)
    {
        int b = following_byte();
        if (b < 0x80 || b >= 0xC0)
        {
            if (b >= 0)
                held_byte = b;                        // cut short: forget the character, keep what came next
            return IGNORED;
        }
        point = (point << 6) | (b & 0x3F);
    }
    return point;
}

// ---- the calls ---------------------------------------------------------------------------------------

int key_get(void)
{
    for (;;)
    {
        if (from_keyboard)
        {
            if (!(mmio_r32(KBD_STATUS) & KBD_KEY_READY))
                return KEYC_NONE;
            return key_from_keystroke(mmio_r32(KBD_KEY));
        }
        int c = next_byte();
        if (c < 0)
            return KEYC_NONE;
        int key = decode_byte(c);
        if (key != IGNORED)
            return key;
    }
}

int key_wait(void)
{
    for (;;)
    {
        if (from_keyboard)
        {
            while (!(mmio_r32(KBD_STATUS) & KBD_KEY_READY))
                __builtin_halt();                   // the keyboard's request ends the halt
            return key_from_keystroke(mmio_r32(KBD_KEY));
        }
        int c = held_byte;
        held_byte = -1;
        if (c < 0)
            c = term_read_char(TERM_READ_UNTIL_STATUS);
        if (c < 0)
            return KEYC_NONE;                         // the input ended
        int key = decode_byte(c & 0xFF);
        if (key != IGNORED)
            return key;
    }
}
