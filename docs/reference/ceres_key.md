# `<ceres/key.h>`

Keystrokes: what a person types, one at a time, in the order they typed it - for a menu, a text field, a game's title screen. The same calls work in a console, in the window and from a file.

```c
  key_start();                                  // ask for keys as they are pressed
  int key = key_wait();
  if (key == KEYC_UP) ...  else if (key == 'q') ...
  key_stop();
```

A KEYSTROKE is an int. A typed character is its Unicode code point (an accented letter is one value, not two bytes), so it compares with a character constant: `key == 'q'`. A key that types no character is a KEYC_* constant, far above the code points so the two never meet.

WHERE THE KEYS COME FROM. A console hands a program whole lines, and only once Enter is pressed, and its own line editor keeps the arrows; reading it with getchar() cannot drive a menu. key_start() asks the host (terminal.h, term_set_raw) to hand the keys over as they are pressed. If it does, they are read from the keyboard device's key register (keyboard.h): in a console, and in the window. If it cannot - the input is a pipe or a file - the keys are decoded from the terminal's bytes instead: a character is itself, Enter is \r or \n, Backspace is 8 or 127, and the arrows are the sequences a terminal sends (ESC [ A ... ESC [ D, or ESC O A ...), so a file can drive a menu the way a person would. Both give the same KEYC_* values.

Nothing echoes: the program draws what it wants shown. While keys are raw the console does not edit lines, so use key_stop() before going back to scanf() or fgets(); the host also puts the console right when the program ends.

```c
#define KEYC_BASE       0x200000                   // above every Unicode code point (at most 0x10FFFF)
#define KEYC(scancode)  (KEYC_BASE + (scancode))   // the keystroke for the key with this scancode (keys.h)

#define KEYC_ENTER      KEYC(KEY_ENTER)            // the keypad's Enter too
#define KEYC_ESC        KEYC(KEY_ESCAPE)
#define KEYC_BACKSPACE  KEYC(KEY_BACKSPACE)
#define KEYC_TAB        KEYC(KEY_TAB)
#define KEYC_INSERT     KEYC(KEY_INSERT)
#define KEYC_HOME       KEYC(KEY_HOME)
#define KEYC_PAGEUP     KEYC(KEY_PAGEUP)
#define KEYC_DELETE     KEYC(KEY_DELETE)
#define KEYC_END        KEYC(KEY_END)
#define KEYC_PAGEDOWN   KEYC(KEY_PAGEDOWN)
#define KEYC_RIGHT      KEYC(KEY_RIGHT)
#define KEYC_LEFT       KEYC(KEY_LEFT)
#define KEYC_DOWN       KEYC(KEY_DOWN)
#define KEYC_UP         KEYC(KEY_UP)
#define KEYC_F(n)       KEYC(KEY_F1 + (n) - 1)     // F1 to F12

#define KEYC_NONE       (-1)                       // key_get(): nothing typed; key_wait(): the input ended

int  key_start(void);                              // ask for keys as they are pressed; 1 if the host will, 0 if they are read from the terminal's bytes
void key_stop(void);                               // give the console back (line editing, echo); calls nest: only the last one does
int  key_get(void);                                // the next keystroke, or KEYC_NONE if there is none yet
int  key_wait(void);                               // waits for one; KEYC_NONE once the input has ended
int  key_from_keystroke(unsigned int keystroke);   // the keyboard register's word as a keystroke as described above
```
