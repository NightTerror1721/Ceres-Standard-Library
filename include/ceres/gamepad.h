// Gamepad device (0xFF080000). Polled rather than event-driven: read the button
// mask and axes every frame.
// See CeresASM docs/07-IO-Devices-and-Ports.md. Without a window nothing ever arrives.

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

#define GP_AXIS_MAX           32767
#define GP_DEFAULT_DEADZONE   4096      // about an eighth of the range: sticks rarely rest at exactly 0

int  gp_changed(void);         // nonzero if the state changed; clears the flag
unsigned int gp_buttons(void); // button bitmask
int  gp_left_x(void);          // signed sticks (-32768..32767)
int  gp_left_y(void);
int  gp_right_x(void);
int  gp_right_y(void);
int  gp_left_trigger(void);    // triggers (0..32767)
int  gp_right_trigger(void);

// Stick handling. gp_deadzone() zeroes |v| <= zone and rescales the rest so the output still reaches
// the full range (a stick just outside the zone reads near 0, not a sudden jump to `zone`).
// gp_axis_f() maps -32768..32767 to -1.0..1.0.
int   gp_deadzone(int v, int zone);
float gp_axis_f(int v);

// Everything at once, with the button edges worked out against the previous call.
struct gp_state
{
    unsigned int buttons;        // held now
    unsigned int pressed;        // went down since the previous gp_poll()
    unsigned int released;       // went up since the previous gp_poll()
    int lx, ly, rx, ry;          // sticks, raw
    int lt, rt;                  // triggers, raw
};

void gp_poll(struct gp_state* out);
