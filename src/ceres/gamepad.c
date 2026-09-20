#include "ceres/gamepad.h"

int gp_changed(void)          { return (mmio_r32(GP_STATUS) & GP_CHANGED) != 0; }
unsigned int gp_buttons(void) { return mmio_r32(GP_BUTTONS); }
int gp_left_x(void)           { return (int)mmio_r32(GP_LEFT_X); }
int gp_left_y(void)           { return (int)mmio_r32(GP_LEFT_Y); }
int gp_right_x(void)          { return (int)mmio_r32(GP_RIGHT_X); }
int gp_right_y(void)          { return (int)mmio_r32(GP_RIGHT_Y); }
int gp_left_trigger(void)     { return (int)mmio_r32(GP_LEFT_TRIG); }
int gp_right_trigger(void)    { return (int)mmio_r32(GP_RIGHT_TRIG); }

int gp_deadzone(int v, int zone)
{
    int magnitude = v < 0 ? -v : v;                      // -32768 -> 32768 is fine in an int
    if (zone < 0)
        zone = 0;
    if (magnitude <= zone || zone >= GP_AXIS_MAX)
        return 0;
    int scaled = (magnitude - zone) * GP_AXIS_MAX / (GP_AXIS_MAX - zone);   // at most ~1.07e9: fits
    if (scaled > GP_AXIS_MAX)
        scaled = GP_AXIS_MAX;
    return v < 0 ? -scaled : scaled;
}

float gp_axis_f(int v)
{
    float f = (float)v / 32767.0f;
    if (f > 1.0f)
        return 1.0f;
    if (f < -1.0f)
        return -1.0f;                                    // -32768 is one step past -1
    return f;
}

static unsigned int previous_buttons;

void gp_poll(struct gp_state* out)
{
    out->buttons = gp_buttons();
    out->pressed = out->buttons & ~previous_buttons;
    out->released = previous_buttons & ~out->buttons;
    previous_buttons = out->buttons;
    out->lx = gp_left_x();
    out->ly = gp_left_y();
    out->rx = gp_right_x();
    out->ry = gp_right_y();
    out->lt = gp_left_trigger();
    out->rt = gp_right_trigger();
}
