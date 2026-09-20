// Mouse device (0xFF060000). Reports deltas (consumed on read), absolute
// position, a button mask and the wheel.
// See CeresASM docs/07-IO-Devices-and-Ports.md.
//
// The deltas and the wheel are CONSUMED by reading them: read each once per frame, ideally all at
// once with mouse_poll(). Without a window nothing ever arrives.

#pragma once

#include "../ceres.h"

#define MOUSE_STATUS   (MOUSE_BASE + 0x00)  // read: bit0 = new data since last status read (clears on read)
#define MOUSE_DX       (MOUSE_BASE + 0x04)  // read: signed X delta since last read (consumed)
#define MOUSE_DY       (MOUSE_BASE + 0x08)  // read: signed Y delta since last read (consumed)
#define MOUSE_X        (MOUSE_BASE + 0x0C)  // read: absolute X
#define MOUSE_Y        (MOUSE_BASE + 0x10)  // read: absolute Y
#define MOUSE_BUTTONS  (MOUSE_BASE + 0x14)  // read: button mask
#define MOUSE_WHEEL    (MOUSE_BASE + 0x18)  // read: signed wheel since last read (consumed)

#define MOUSE_MOVED       0x01
#define MOUSE_BTN_LEFT    0x01
#define MOUSE_BTN_RIGHT   0x02
#define MOUSE_BTN_MIDDLE  0x04

int mouse_moved(void);      // nonzero if new data arrived; clears the flag
int mouse_dx(void);         // signed delta, consumed on read
int mouse_dy(void);
int mouse_x(void);          // absolute position
int mouse_y(void);
int mouse_wheel(void);      // signed, consumed on read
int mouse_buttons(void);    // mask: MOUSE_BTN_*

// Everything at once, with the button edges worked out against the previous call.
struct mouse_state
{
    int x, y;                    // absolute position
    int dx, dy;                  // movement since the previous mouse_poll()
    int wheel;                   // wheel movement since the previous mouse_poll()
    unsigned int buttons;        // held now
    unsigned int pressed;        // went down since the previous poll
    unsigned int released;       // went up since the previous poll
};

void mouse_poll(struct mouse_state* out);
