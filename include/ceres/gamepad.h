// Gamepad device (0xFF080000). Polled rather than event-driven: read the button
// mask and axes every frame.
// See CeresASM docs/07-IO-Devices-and-Ports.md.

#pragma once

#include "../ceres.h"

#define GP_STATUS     (GAMEPAD_BASE + 0x00)  // read: bit0 = state changed since last status read (clears on read)
#define GP_BUTTONS    (GAMEPAD_BASE + 0x04)  // read: button bitmask
#define GP_LEFT_X     (GAMEPAD_BASE + 0x08)  // read: signed left stick X
#define GP_LEFT_Y     (GAMEPAD_BASE + 0x0C)  // read: signed left stick Y
#define GP_RIGHT_X    (GAMEPAD_BASE + 0x10)  // read: signed right stick X
#define GP_RIGHT_Y    (GAMEPAD_BASE + 0x14)  // read: signed right stick Y
#define GP_LEFT_TRIG  (GAMEPAD_BASE + 0x18)  // read: left trigger (0..32767)
#define GP_RIGHT_TRIG (GAMEPAD_BASE + 0x1C)  // read: right trigger (0..32767)

#define GP_CHANGED 0x01

// Buttons (south = A, east = B, west = X, north = Y).
#define GP_BTN_SOUTH          0x0001
#define GP_BTN_EAST           0x0002
#define GP_BTN_WEST           0x0004
#define GP_BTN_NORTH          0x0008
#define GP_BTN_BACK           0x0010
#define GP_BTN_GUIDE          0x0020
#define GP_BTN_START          0x0040
#define GP_BTN_LEFT_STICK     0x0080
#define GP_BTN_RIGHT_STICK    0x0100
#define GP_BTN_LEFT_SHOULDER  0x0200
#define GP_BTN_RIGHT_SHOULDER 0x0400
#define GP_BTN_DPAD_UP        0x0800
#define GP_BTN_DPAD_DOWN      0x1000
#define GP_BTN_DPAD_LEFT      0x2000
#define GP_BTN_DPAD_RIGHT     0x4000

int  gp_changed(void);         // nonzero if the state changed; clears the flag
unsigned int gp_buttons(void); // button bitmask
int  gp_left_x(void);          // signed sticks (-32768..32767)
int  gp_left_y(void);
int  gp_right_x(void);
int  gp_right_y(void);
int  gp_left_trigger(void);    // triggers (0..32767)
int  gp_right_trigger(void);
