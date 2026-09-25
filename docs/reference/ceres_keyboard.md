# `<ceres/keyboard.h>`

Keyboard device (0xFF050000). Reports key *events* - a code plus a pressed/released flag - unlike the terminal's character stream. See CeresASM docs/07-IO-Devices-and-Ports.md. Codes are in ceres/keys.h.

The device holds 64 events; a full queue DROPS new ones, so drain it every frame. Each event also raises interrupt 19 (IRQ_KEYBOARD), which a program may attach a handler to (ceres/irq.h). Nothing arrives from a plain `ceres run` on a pipe or a file. On a console the keys arrive once the program asks for them as they are pressed (term_set_raw(), or key_start() in ceres/key.h, which is what to read for a menu or a text field); in the window they always do.

```c
#define KBD_STATUS         (KEYBOARD_BASE + 0x00)  // read: bit0 set when an event is queued
#define KBD_EVENT          (KEYBOARD_BASE + 0x04)  // read: pop one event
#define KBD_TEXT           (KEYBOARD_BASE + 0x08)  // read: pop one typed character, as a Unicode code point
#define KBD_KEY            (KEYBOARD_BASE + 0x0C)  // read: pop the next keystroke, in typing order (see below)
#define KBD_BLOCK_READ_CNT (KEYBOARD_BASE + 0x10)  // read: events the last block read drained
#define KBD_BLOCK_ADDR     (KEYBOARD_BASE + 0xF0)
#define KBD_BLOCK_LEN      (KEYBOARD_BASE + 0xF4)
#define KBD_BLOCK_CMD      (KEYBOARD_BASE + 0xF8)  // write: 1 drains the queue into RAM

#define KBD_EVENT_READY     0x01
#define KBD_TEXT_READY      0x02              // a typed character is queued
#define KBD_KEY_READY       0x04              // a keystroke is queued
#define KBD_KEY_NAMED       0x80000000u       // in a keystroke: a key with no character; the low bits are its scancode
#define KBD_EVENT_PRESSED   0x80000000u   // bit31: 1 = pressed, 0 = released
#define KBD_EVENT_CODE_MASK 0x7FFFFFFFu   // bits 30:0 hold the key code
#define KBD_BLOCK_CMD_READ  0x01
#define KBD_QUEUE_SIZE      64            // events the device can hold

// KEYSTROKES. The event queue and the text queue are separate, so a program reading both cannot tell whether the
// letter or the Enter came first. KBD_KEY is one ordered stream: a typed character is its Unicode code point,
// and a press of a key with no character (Enter, Escape, Backspace, Tab, the arrows, Home, End, PageUp,
// PageDown, Insert, Delete, F1-F12) is KBD_KEY_NAMED | its scancode. Ceres/key.h wraps it.

int  kbd_event_ready(void);              // nonzero when an event is queued
int  kbd_text_ready(void);               // nonzero when a typed character is queued
unsigned int kbd_read_text(void);        // pop one typed character (a Unicode code point); 0 when empty
unsigned int kbd_read_event(void);       // pop one event; 0 when empty
int  kbd_is_pressed(unsigned int event); // nonzero if the pressed bit is set
int  kbd_keycode(unsigned int event);    // bits 30:0

// Drains up to `max` events (at most KBD_QUEUE_SIZE) into `events` with ONE block transfer, oldest
// first, and returns how many. 0 when the queue is empty.
int  kbd_read_block(unsigned int* events, int max);
int  kbd_flush(void);                    // throws every queued event away; returns how many
```
