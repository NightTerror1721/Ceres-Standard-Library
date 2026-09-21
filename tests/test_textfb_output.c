// Where a frame of the text framebuffer goes: the window where the host has one, the terminal where it has not,
// and the program's own say in between. The tests run with CERES_HEADLESS set (tools/runtests.ps1 does it), so
// there is no window: a frame always ends up on the terminal, and what the program asks for shows in the mode
// register but not in where the frame goes.
#include "ceres/test.h"
#include "ceres/textfb.h"

int main(void)
{
    TEST_SECTION("the default");
    CHECK_EQ(fb_init(4, 2), 0);
    CHECK_EQ((int)mmio_r32(FB_MODE), FB_OUT_AUTO);
    CHECK_EQ(fb_output(), FB_OUT_TERMINAL);               // no window in this run
    fb_text(0, 0, "ab");
    fb_present();

    TEST_SECTION("asking for the window where there is none");
    fb_set_output(FB_OUT_WINDOW);
    CHECK_EQ((int)mmio_r32(FB_MODE), FB_OUT_WINDOW);      // what was asked for
    CHECK_EQ(fb_output(), FB_OUT_TERMINAL);               // what happens: the frame is still shown
    fb_text(0, 1, "cd");
    fb_present();

    TEST_SECTION("asking for the terminal");
    fb_set_output(FB_OUT_TERMINAL);
    CHECK_EQ((int)mmio_r32(FB_MODE), FB_OUT_TERMINAL);
    CHECK_EQ(fb_output(), FB_OUT_TERMINAL);
    fb_clear('.');
    fb_present();

    TEST_SECTION("back to the default, and what is not a mode");
    fb_set_output(FB_OUT_AUTO);
    CHECK_EQ((int)mmio_r32(FB_MODE), FB_OUT_AUTO);
    fb_set_output(FB_OUT_TERMINAL);
    fb_set_output(7);                                     // not a mode: ignored
    fb_set_output(-1);
    CHECK_EQ((int)mmio_r32(FB_MODE), FB_OUT_TERMINAL);
    fb_set_output(FB_OUT_AUTO);
    fb_shutdown();
    return test_summary();
}
