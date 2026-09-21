// USE: irq
// The Mandelbrot set on the 320 x 200 pixel display, in single-precision float.
//
//   arrows          pan             Z / +   zoom in        X / -   zoom out
//   R               back to the start                      Esc     quit
//
// The picture is drawn a band of rows at a time and shown as it grows. Zooming in raises the iteration
// limit, since the boundary needs more of it the closer you look.
//
// Built with -DDEMO_FRAMES=1 it draws the first view once, without a window, and prints how many pixels are
// in the set, the sum of every pixel's iteration count and a CRC-32 of the finished frame: that is what
// examples/expected/mandelbrot.expected holds, checked against the same computation done outside the VM.
#include "stdio.h"
#include "ceres/gfx.h"
#include "ceres/font.h"
#include "ceres/game.h"
#include "ceres/input.h"
#include "ceres/hash.h"

#define W 320
#define H 200

static float center_x, center_y, scale;     // scale: how much of the plane one pixel covers
static int zoom;                            // how many times it has been zoomed in
static long inside_count;
static long total_iterations;

static void reset_view(void)
{
    center_x = -0.5f;
    center_y = 0.0f;
    scale = 0.01f;
    zoom = 0;
}

static int max_iterations(void)
{
    int n = 48 + 12 * zoom;
    return n > 400 ? 400 : n;
}

static int escape_time(float cr, float ci, int limit)
{
    float zr = 0.0f, zi = 0.0f;
    int n = 0;
    while (n < limit)
    {
        float zr2 = zr * zr;
        float zi2 = zi * zi;
        if (zr2 + zi2 > 4.0f)
            break;
        zi = 2.0f * zr * zi + ci;
        zr = zr2 - zi2 + cr;
        n++;
    }
    return n;
}

// Draws rows [from, to) and returns nothing; the caller presents between bands.
static void render_rows(int from, int to)
{
    int limit = max_iterations();
    for (int py = from; py < to; py++)
        for (int px = 0; px < W; px++)
        {
            float cr = center_x + (float)(px - W / 2) * scale;
            float ci = center_y + (float)(py - H / 2) * scale;
            int n = escape_time(cr, ci, limit);
            total_iterations += n;
            if (n == limit)
            {
                inside_count++;
                gfx_pixel(px, py, RGB(0, 0, 0));
            }
            else
                gfx_pixel(px, py, color_hsv(n * 9, 255, 255));
        }
}

static void render_all(int present_each_band)
{
    inside_count = 0;
    total_iterations = 0;
    for (int y = 0; y < H; y += 20)
    {
        render_rows(y, y + 20);
        if (present_each_band)
            gfx_present();
    }
    gfx_present();
}

int main(void)
{
    if (gfx_init(W, H) != 0)
    {
        puts("mandelbrot: no display");
        return 1;
    }
    reset_view();

#ifdef DEMO_FRAMES
    render_all(0);
    printf("inside %ld, iterations %ld, crc %08x\n", inside_count, total_iterations,
           hash_crc32(gfx_screen()->px, W * H * 4));
    return 0;
#else
    struct game g;
    game_init(&g, 0);
    game_pace_ms(&g, 50);
    input_terminal_fallback(1);
    render_all(1);
    while (g.running)
    {
        game_frame_begin(&g);
        int changed = 0;
        float step = scale * 40.0f;                     // 40 pixels
        if (key_pressed(KEY_ESCAPE))
            game_quit(&g);
        if (key_pressed(KEY_LEFT))  { center_x -= step; changed = 1; }
        if (key_pressed(KEY_RIGHT)) { center_x += step; changed = 1; }
        if (key_pressed(KEY_UP))    { center_y -= step; changed = 1; }
        if (key_pressed(KEY_DOWN))  { center_y += step; changed = 1; }
        if (key_pressed(KEY_Z) || key_pressed(KEY_EQUALS)) { scale *= 0.5f; zoom++; changed = 1; }
        if ((key_pressed(KEY_X) || key_pressed(KEY_MINUS)) && zoom > 0) { scale *= 2.0f; zoom--; changed = 1; }
        if (key_pressed(KEY_R)) { reset_view(); changed = 1; }
        if (changed)
            render_all(1);
        game_frame_end(&g);
    }
    return 0;
#endif
}
