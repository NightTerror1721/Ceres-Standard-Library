#include "ceres/input.h"
#include "ceres/keyboard.h"
#include "stdio.h"

#define K_DOWN      1     // held now
#define K_PRESSED   2     // went down this frame
#define K_RELEASED  4     // went up this frame
#define K_SYNTH     8     // came from the terminal: it lasts one frame

static unsigned char keys[INPUT_MAX_KEYS];
static struct mouse_state mouse;
static struct gp_state pad;
static int terminal_mode;

// ---- keyboard state ----

void input_begin_frame(void)
{
    for (int i = 0; i < INPUT_MAX_KEYS; i++)
    {
        unsigned char k = keys[i];
        if (k & K_SYNTH)
        {
            // a terminal keystroke has no release: it simply ends with the frame it arrived in
            k = (k & K_DOWN) ? K_RELEASED : 0;
        }
        else
        {
            k = (unsigned char)(k & K_DOWN);        // keep "held"; drop last frame's edges
        }
        keys[i] = k;
    }
}

void input_key_event(int scancode, int pressed)
{
    if (scancode <= 0 || scancode >= INPUT_MAX_KEYS)
        return;
    unsigned char k = keys[scancode];
    if (pressed)
    {
        if (!(k & K_DOWN))
            k |= K_PRESSED;                         // a repeat of a key already down is not a new press
        k |= K_DOWN;
    }
    else if (k & K_DOWN)
    {
        k = (unsigned char)((k & ~K_DOWN) | K_RELEASED);
    }
    keys[scancode] = k;
}

int key_down(int scancode)     { return scancode > 0 && scancode < INPUT_MAX_KEYS && (keys[scancode] & K_DOWN) != 0; }
int key_pressed(int scancode)  { return scancode > 0 && scancode < INPUT_MAX_KEYS && (keys[scancode] & K_PRESSED) != 0; }
int key_released(int scancode) { return scancode > 0 && scancode < INPUT_MAX_KEYS && (keys[scancode] & K_RELEASED) != 0; }

int key_any_pressed(void)
{
    for (int i = 1; i < INPUT_MAX_KEYS; i++)
        if (keys[i] & K_PRESSED)
            return i;
    return 0;
}

// ---- characters <-> keys (US layout) ----

// keys 45..56, in order: - = [ ] \ (non-US #) ; ' ` , . /
static const char punct_plain[] = "-=[]\\#;'`,./";
static const char punct_shift[] = "_+{}|~:\"~<>?";
static const char digit_shift[] = "!@#$%^&*()";      // shifted 1 2 3 4 5 6 7 8 9 0

int input_text_ready(void)
{
    return kbd_text_ready();
}

unsigned int input_text(void)
{
    return kbd_read_text();
}

int key_to_ascii(int scancode, int shift)
{
    if (scancode >= KEY_A && scancode <= KEY_Z)
        return (shift ? 'A' : 'a') + (scancode - KEY_A);
    if (scancode >= KEY_1 && scancode <= KEY_0)
    {
        int i = scancode - KEY_1;                   // 0..9, with 0 last
        return shift ? digit_shift[i] : (i == 9 ? '0' : '1' + i);
    }
    if (scancode >= KEY_MINUS && scancode <= KEY_SLASH)
        return shift ? punct_shift[scancode - KEY_MINUS] : punct_plain[scancode - KEY_MINUS];
    if (scancode >= KEY_KP_1 && scancode <= KEY_KP_9)
        return '1' + (scancode - KEY_KP_1);
    switch (scancode)
    {
    case KEY_ENTER:
    case KEY_KP_ENTER:    return '\n';
    case KEY_ESCAPE:      return 27;
    case KEY_BACKSPACE:   return 8;
    case KEY_TAB:         return '\t';
    case KEY_SPACE:       return ' ';
    case KEY_KP_0:        return '0';
    case KEY_KP_PERIOD:   return '.';
    case KEY_KP_DIVIDE:   return '/';
    case KEY_KP_MULTIPLY: return '*';
    case KEY_KP_MINUS:    return '-';
    case KEY_KP_PLUS:     return '+';
    }
    return 0;
}

// The key that types `c`, and whether it needs shift; 0 when there is none.
static int key_from_char(int c, int* needs_shift)
{
    *needs_shift = 0;
    if (c == '\r')
        c = '\n';
    if (c == 127)
        c = 8;
    for (int sc = KEY_A; sc <= KEY_KP_PERIOD; sc++)
    {
        if (key_to_ascii(sc, 0) == c)
            return sc;
    }
    for (int sc = KEY_A; sc <= KEY_SLASH; sc++)
    {
        if (key_to_ascii(sc, 1) == c)
        {
            *needs_shift = 1;
            return sc;
        }
    }
    return 0;
}

static void synth_press(int scancode)
{
    unsigned char k = keys[scancode];
    if (!(k & K_DOWN))
        k |= K_PRESSED;
    keys[scancode] = (unsigned char)(k | K_DOWN | K_SYNTH);
}

void input_terminal_fallback(int enable)
{
    terminal_mode = enable != 0;
}

// ---- mouse and gamepad ----

int mouse_pos_x(void)   { return mouse.x; }
int mouse_pos_y(void)   { return mouse.y; }
int mouse_move_x(void)  { return mouse.dx; }
int mouse_move_y(void)  { return mouse.dy; }
int mouse_scroll(void)  { return mouse.wheel; }
int mouse_btn_down(int mask)     { return (mouse.buttons & (unsigned int)mask) != 0; }
int mouse_btn_pressed(int mask)  { return (mouse.pressed & (unsigned int)mask) != 0; }
int mouse_btn_released(int mask) { return (mouse.released & (unsigned int)mask) != 0; }

int pad_down(unsigned int mask)     { return mask != 0 && (pad.buttons & mask) == mask; }
int pad_pressed(unsigned int mask)  { return (pad.pressed & mask) != 0; }
int pad_released(unsigned int mask) { return (pad.released & mask) != 0; }

float pad_axis(int which)
{
    switch (which)
    {
    case PAD_AXIS_LEFT_X:  return gp_axis_f(gp_deadzone(pad.lx, GP_DEFAULT_DEADZONE));
    case PAD_AXIS_LEFT_Y:  return gp_axis_f(gp_deadzone(pad.ly, GP_DEFAULT_DEADZONE));
    case PAD_AXIS_RIGHT_X: return gp_axis_f(gp_deadzone(pad.rx, GP_DEFAULT_DEADZONE));
    case PAD_AXIS_RIGHT_Y: return gp_axis_f(gp_deadzone(pad.ry, GP_DEFAULT_DEADZONE));
    case PAD_AXIS_LEFT_TRIGGER:  return (float)pad.lt / (float)GP_AXIS_MAX;
    case PAD_AXIS_RIGHT_TRIGGER: return (float)pad.rt / (float)GP_AXIS_MAX;
    }
    return 0.0f;
}

// ---- the frame ----

void input_init(void)
{
    for (int i = 0; i < INPUT_MAX_KEYS; i++)
        keys[i] = 0;
    kbd_flush();
    mouse_poll(&mouse);                              // the first poll only establishes the button baseline
    gp_poll(&pad);
    mouse.dx = mouse.dy = mouse.wheel = 0;
    mouse.pressed = mouse.released = 0;
    pad.pressed = pad.released = 0;
    terminal_mode = 0;
}

void input_update(void)
{
    input_begin_frame();

    unsigned int events[KBD_QUEUE_SIZE];
    int n = kbd_read_block(events, KBD_QUEUE_SIZE);
    for (int i = 0; i < n; i++)
        input_key_event(kbd_keycode(events[i]), kbd_is_pressed(events[i]));

    mouse_poll(&mouse);
    gp_poll(&pad);

    if (terminal_mode)
    {
        for (;;)
        {
            int c = getchar_nb();
            if (c < 0)
                break;
            int shift;
            int sc = key_from_char(c, &shift);
            if (sc == 0)
                continue;
            if (shift)
                synth_press(KEY_LSHIFT);
            synth_press(sc);
        }
    }
}
