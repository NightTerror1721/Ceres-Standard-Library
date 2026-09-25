// The blitter (ceres/blitter.h) and gfx on it: the same drawing done by the blitter and in software comes out the
// same pixel for pixel, and what software does a pixel at a time (keyed and scaled blits) costs a fraction of the
// instructions; and the display's indexed mode and scroll.
#include "ceres/test.h"
#include "ceres/gfx.h"
#include "ceres/blitter.h"
#include "ceres/display.h"
#include "ceres/timer.h"
#include "string.h"

static unsigned int seed = 7;
static int rnd(int n)
{
    seed = seed * 1103515245u + 12345u;
    return (int)((seed >> 16) % (unsigned int)n);
}

// A run of random operations on `target`, the same run whenever seed starts the same.
static void scene(struct gfx_surface* target, struct gfx_surface* pattern)
{
    gfx_set_target(target);
    gfx_clear(0x00102030u);
    for (int i = 0; i < 40; i++)
    {
        int kind = rnd(5);
        int x = rnd(80) - 10, y = rnd(60) - 10, w = rnd(40) + 1, h = rnd(30) + 1;
        if (i == 20)
            gfx_set_clip(5, 5, 50, 40);                    // halfway through, a clip
        if (kind == 0)
            gfx_rect_fill(x, y, w, h, (unsigned int)rnd(0x1000000));
        else if (kind == 1)
            gfx_blit(pattern, rnd(20), rnd(20), w, h, x, y);
        else if (kind == 2)
            gfx_blit_key(pattern, rnd(20), rnd(20), w, h, x, y, 0x00FF00FFu);
        else if (kind == 3)
            gfx_blit_scaled(pattern, x, y, rnd(3) + 1);
        else
            gfx_blit(target, rnd(40), rnd(30), w, h, x, y);   // onto itself, overlapping
    }
    gfx_reset_clip();
}

int main(void)
{
    TEST_SECTION("there is one");
    CHECK_EQ(blitter_available(), 1);
    CHECK_EQ(gfx_init(64, 48), 0);

    TEST_SECTION("the same pixels, by the blitter and in software");
    struct gfx_surface* pattern = gfx_surface_new(24, 24);
    gfx_set_target(pattern);
    for (int y = 0; y < 24; y++)
        for (int x = 0; x < 24; x++)
            gfx_pixel(x, y, (x + y) % 5 == 0 ? 0x00FF00FFu : (unsigned int)(x * 10 << 16 | y * 10 << 8 | 0x40));
    struct gfx_surface* fast = gfx_surface_new(80, 60);
    struct gfx_surface* slow = gfx_surface_new(80, 60);
    seed = 7;
    scene(fast, pattern);
    gfx_use_blitter(0);
    seed = 7;
    scene(slow, pattern);
    gfx_use_blitter(1);
    CHECK(memcmp(fast->px, slow->px, 80 * 60 * 4) == 0);

    TEST_SECTION("what software does a pixel at a time");
    gfx_set_target(fast);
    uint64_t t0 = timer_ticks64();
    gfx_blit_key(pattern, 0, 0, 24, 24, 10, 10, 0x00FF00FFu);
    gfx_blit_scaled(pattern, 0, 0, 2);
    uint64_t t1 = timer_ticks64();
    gfx_use_blitter(0);
    gfx_set_target(slow);
    gfx_blit_key(pattern, 0, 0, 24, 24, 10, 10, 0x00FF00FFu);
    gfx_blit_scaled(pattern, 0, 0, 2);
    uint64_t t2 = timer_ticks64();
    gfx_use_blitter(1);
    CHECK(memcmp(fast->px, slow->px, 80 * 60 * 4) == 0);
    CHECK((t1 - t0) * 10 < (t2 - t1));                   // 576 pixels keyed and 2304 scaled, one by one
    gfx_set_target(fast);
    gfx_clear(0x00ABCDEFu);
    CHECK(fast->px[0] == 0x00ABCDEFu && fast->px[80 * 60 - 1] == 0x00ABCDEFu);

    TEST_SECTION("indexed pixels through a palette");
    static unsigned int palette[256];
    palette[1] = 0x00FF0000u;
    palette[2] = 0x0000FF00u;
    unsigned char idx[6] = { 1, 2, 0, 2, 1, 0 };
    unsigned int out[6];
    memset(out, 0, sizeof out);
    CHECK_EQ(blitter_copy_indexed(out, 12, idx, 3, 3, 2, palette, 0), 0);   // index 0 left out
    CHECK(out[0] == 0x00FF0000u && out[1] == 0x0000FF00u && out[2] == 0u && out[4] == 0x00FF0000u);
    CHECK_EQ((int)blitter_last_pixels(), 4);
    CHECK_EQ(blitter_fill((unsigned int*)0x7FFFFFF0u, 16, 4, 4, 0), -1);     // outside RAM

    TEST_SECTION("the display's indexed mode and scroll");
    CHECK_EQ(display_set_mode(DISP_MODE_INDEXED), 0);
    display_load_palette(palette, 3);
    static unsigned char frame8[64 * 48];
    for (int i = 0; i < 64 * 48; i++)
        frame8[i] = (unsigned char)(i % 3);
    display_blit8(frame8, 64, 48);
    display_scroll(-1, 50);                              // 63, 2
    CHECK_EQ((int)mmio_r32(DISP_SCROLL_X), 63);
    CHECK_EQ((int)mmio_r32(DISP_SCROLL_Y), 2);
    display_show();
    CHECK_EQ(display_set_mode(DISP_MODE_RGB32), 0);
    display_scroll(0, 0);
    gfx_surface_free(pattern);
    gfx_surface_free(fast);
    gfx_surface_free(slow);
    gfx_shutdown();
    return test_summary();
}
