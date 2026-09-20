#pragma once

#include "keys.h"
#include "mouse.h"
#include "gamepad.h"

// Input for a frame loop: the state of the keyboard, the mouse and the gamepad as of the last
// input_update(), with "went down this frame" and "went up this frame" worked out for each.
//
//   input_init();
//   for (;;) {
//       input_update();                        // ONCE per frame: drains the devices
//       if (key_pressed(KEY_SPACE)) jump();
//       if (key_down(KEY_LEFT))     x -= 2;
//       ...
//   }
//
// A key that goes down AND up between two updates still counts as pressed for that frame (a quick tap
// is not lost), though key_down() will say it is not held.
//
// TERMINAL MODE. Without `--window` the keyboard device never receives anything: what the user types
// arrives through the terminal instead. input_terminal_fallback(1) makes input_update() also read the
// terminal (without waiting) and turn each character into a one-frame press of the matching key
// (a-z, 0-9, space, Enter, Escape, punctuation; upper case also presses KEY_LSHIFT). A game written
// against key_pressed()/key_down() then runs in a plain terminal.

#define INPUT_MAX_KEYS 256

void input_init(void);                   // forgets everything, drains the devices, terminal fallback off
void input_update(void);                 // call ONCE per frame
void input_terminal_fallback(int enable);

// The two halves of input_update(), for tests and for replaying recorded input: begin_frame() clears
// the per-frame edges, key_event() records one event.
void input_begin_frame(void);
void input_key_event(int scancode, int pressed);

// ---- keyboard ----
int key_down(int scancode);              // held now
int key_pressed(int scancode);           // went down since the previous update
int key_released(int scancode);          // went up since the previous update
int key_any_pressed(void);               // the first scancode that went down this frame, or 0
int key_to_ascii(int scancode, int shift);   // US layout; 0 for a key with no character (arrows, F1...)

// ---- mouse ----
int mouse_pos_x(void);
int mouse_pos_y(void);
int mouse_move_x(void);                  // movement during this frame
int mouse_move_y(void);
int mouse_scroll(void);                  // wheel movement during this frame
int mouse_btn_down(int mask);            // any button of the mask (MOUSE_BTN_*) is held
int mouse_btn_pressed(int mask);
int mouse_btn_released(int mask);

// ---- gamepad ----
#define PAD_AXIS_LEFT_X         0
#define PAD_AXIS_LEFT_Y         1
#define PAD_AXIS_RIGHT_X        2
#define PAD_AXIS_RIGHT_Y        3
#define PAD_AXIS_LEFT_TRIGGER   4
#define PAD_AXIS_RIGHT_TRIGGER  5

int   pad_down(unsigned int mask);       // ALL the buttons of the mask (GP_BTN_*) are held
int   pad_pressed(unsigned int mask);    // ANY of them went down this frame
int   pad_released(unsigned int mask);
float pad_axis(int which);               // sticks -1.0..1.0 with a dead zone; triggers 0.0..1.0
