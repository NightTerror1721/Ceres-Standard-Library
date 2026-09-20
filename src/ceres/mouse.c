#include "ceres/mouse.h"

int mouse_moved(void)   { return (mmio_r32(MOUSE_STATUS) & MOUSE_MOVED) != 0; }
int mouse_dx(void)      { return (int)mmio_r32(MOUSE_DX); }
int mouse_dy(void)      { return (int)mmio_r32(MOUSE_DY); }
int mouse_x(void)       { return (int)mmio_r32(MOUSE_X); }
int mouse_y(void)       { return (int)mmio_r32(MOUSE_Y); }
int mouse_wheel(void)   { return (int)mmio_r32(MOUSE_WHEEL); }
int mouse_buttons(void) { return (int)mmio_r32(MOUSE_BUTTONS); }

static unsigned int previous_buttons;

void mouse_poll(struct mouse_state* out)
{
    // The deltas and the wheel are consumed by these reads, so each is read exactly once.
    out->dx = mouse_dx();
    out->dy = mouse_dy();
    out->wheel = mouse_wheel();
    out->x = mouse_x();
    out->y = mouse_y();
    out->buttons = (unsigned int)mouse_buttons();
    out->pressed = out->buttons & ~previous_buttons;
    out->released = previous_buttons & ~out->buttons;
    previous_buttons = out->buttons;
}
