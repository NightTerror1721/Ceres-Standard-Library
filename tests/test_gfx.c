// Graphics without a window: everything is drawn into RAM surfaces and READ BACK, so the checks are on
// the actual pixels - exact lines, symmetric circles, filled shapes against their geometry, and above all
// that nothing is ever written outside the clip rectangle.
#include "ceres/test.h"
#include "ceres/gfx.h"
#include "ceres/font.h"
#include "ceres/sprite.h"
#include "ceres/color.h"

#define BG   0x000000u
#define FG   0xFFFFFFu
#define GUARD 0x123456u

static unsigned int pix(const struct gfx_surface* s, int x, int y) { return s->px[y * s->w + x]; }

static int count(const struct gfx_surface* s, unsigned int c)
{
    int n = 0;
    for (int i = 0; i < s->w * s->h; i++)
        if (s->px[i] == c) n++;
    return n;
}

static int same_surface(const struct gfx_surface* a, const struct gfx_surface* b)
{
    if (a->w != b->w || a->h != b->h) return 0;
    for (int i = 0; i < a->w * a->h; i++)
        if (a->px[i] != b->px[i]) return 0;
    return 1;
}

// A fresh black surface made the target.
static struct gfx_surface* canvas(int w, int h)
{
    struct gfx_surface* s = gfx_surface_new(w, h);
    gfx_set_target(s);
    return s;
}

static void done(struct gfx_surface* s)
{
    gfx_surface_free(s);
}

// ---- colours ----

static void colours(void)
{
    TEST_SECTION("colour packing");
    CHECK_EQ((int)RGB(1, 2, 3), 0x010203);
    CHECK_EQ(COL_R(0x123456), 0x12);
    CHECK_EQ(COL_G(0x123456), 0x34);
    CHECK_EQ(COL_B(0x123456), 0x56);

    TEST_SECTION("hsv");
    CHECK_EQ((int)color_hsv(0, 255, 255), 0xFF0000);
    CHECK_EQ((int)color_hsv(60, 255, 255), 0xFFFF00);
    CHECK_EQ((int)color_hsv(120, 255, 255), 0x00FF00);
    CHECK_EQ((int)color_hsv(180, 255, 255), 0x00FFFF);
    CHECK_EQ((int)color_hsv(240, 255, 255), 0x0000FF);
    CHECK_EQ((int)color_hsv(300, 255, 255), 0xFF00FF);
    CHECK_EQ((int)color_hsv(0, 0, 200), 0xC8C8C8);      // no saturation: a gray whatever the hue
    CHECK_EQ((int)color_hsv(200, 0, 200), 0xC8C8C8);
    CHECK_EQ((int)color_hsv(120, 255, 0), 0x000000);    // no value: black
    CHECK_EQ((int)color_hsv(360, 255, 255), (int)color_hsv(0, 255, 255));   // the hue wraps
    CHECK_EQ((int)color_hsv(-60, 255, 255), (int)color_hsv(300, 255, 255));
    CHECK_EQ((int)color_hsv(420, 255, 255), (int)color_hsv(60, 255, 255));
    CHECK_EQ((int)color_hsv(0, 999, 999), 0xFF0000);    // saturation and value are clamped
    CHECK_EQ((int)color_hsv(0, -5, 100), 0x646464);
    CHECK_EQ((int)color_hsv(30, 255, 255), 0xFF8000);   // halfway between red and yellow
    CHECK_EQ((int)color_hsv(0, 128, 255), 0xFF7F7F);    // half saturated: washed out

    TEST_SECTION("mixing");
    CHECK_EQ((int)color_lerp(0x000000, 0xFFFFFF, 0), 0x000000);
    CHECK_EQ((int)color_lerp(0x000000, 0xFFFFFF, 255), 0xFFFFFF);
    CHECK_EQ((int)color_lerp(0x000000, 0xFFFFFF, 128), 0x808080);
    CHECK_EQ((int)color_lerp(0xFF0000, 0x0000FF, 0), 0xFF0000);
    CHECK_EQ((int)color_lerp(0xFF0000, 0x0000FF, 255), 0x0000FF);
    CHECK_EQ((int)color_lerp(0x102030, 0x102030, 77), 0x102030);   // the same colour stays
    CHECK_EQ((int)color_lerp(0, 0xFFFFFF, -50), 0x000000);         // t is clamped
    CHECK_EQ((int)color_lerp(0, 0xFFFFFF, 999), 0xFFFFFF);
    CHECK_EQ((int)color_blend(0x204060, 0xFFFFFF, 0), 0x204060);
    CHECK_EQ((int)color_blend(0x204060, 0xFFFFFF, 255), 0xFFFFFF);
    CHECK_EQ((int)color_scale(0x804020, 256), 0x804020);
    CHECK_EQ((int)color_scale(0x804020, 128), 0x402010);
    CHECK_EQ((int)color_scale(0x804020, 0), 0x000000);
    CHECK_EQ((int)color_scale(0x804020, 512), 0xFF8040);
    CHECK_EQ((int)color_scale(0xC0C0C0, 1000), 0xFFFFFF);          // clamped, not wrapped
    CHECK_EQ((int)color_scale(0x804020, -3), 0x000000);

    TEST_SECTION("palette");
    CHECK_EQ((int)PALETTE16[0], 0x000000);
    CHECK_EQ((int)PALETTE16[1], 0x0000AA);
    CHECK_EQ((int)PALETTE16[6], 0xAA5500);              // brown, the one CGA oddity
    CHECK_EQ((int)PALETTE16[7], 0xAAAAAA);
    CHECK_EQ((int)PALETTE16[8], 0x555555);
    CHECK_EQ((int)PALETTE16[15], 0xFFFFFF);
    int distinct = 1;
    for (int i = 0; i < 16; i++)
        for (int k = i + 1; k < 16; k++)
            if (PALETTE16[i] == PALETTE16[k]) distinct = 0;
    CHECK(distinct);
}

// ---- the screen ----

static void screen(void)
{
    TEST_SECTION("before init");
    CHECK(gfx_screen() == NULL);
    CHECK(gfx_target() == NULL);
    CHECK_EQ(gfx_width(), 0);
    gfx_pixel(1, 1, FG);                                // drawing with nowhere to draw is a no-op, not a crash
    gfx_present();

    TEST_SECTION("init");
    CHECK_EQ(gfx_init(0, 10), -1);
    CHECK_EQ(gfx_init(10, -1), -1);
    CHECK_EQ(gfx_init(1281, 10), -1);
    CHECK_EQ(gfx_init(10, 721), -1);
    CHECK(gfx_screen() == NULL);                        // a refused size changed nothing
    CHECK_EQ(gfx_init(64, 48), 0);
    CHECK(gfx_screen() != NULL);
    CHECK(gfx_target() == gfx_screen());
    CHECK_EQ(gfx_width(), 64);
    CHECK_EQ(gfx_height(), 48);
    CHECK_EQ(count(gfx_screen(), BG), 64 * 48);         // it starts black

    TEST_SECTION("pixels");
    gfx_clear(0x112233);
    CHECK_EQ(count(gfx_screen(), 0x112233), 64 * 48);
    gfx_pixel(3, 4, FG);
    CHECK_EQ((int)gfx_get(3, 4), (int)FG);
    CHECK_EQ((int)gfx_get(4, 3), 0x112233);
    CHECK_EQ((int)gfx_get(-1, 0), 0);                   // outside reads as 0
    CHECK_EQ((int)gfx_get(64, 0), 0);
    CHECK_EQ((int)gfx_get(0, 48), 0);
    gfx_pixel(-1, 0, FG);                               // and outside writes are dropped
    gfx_pixel(64, 47, FG);
    gfx_pixel(0, 48, FG);
    gfx_pixel(-1000000, 5, FG);
    gfx_pixel(5, 1000000, FG);
    CHECK_EQ(count(gfx_screen(), FG), 1);

    TEST_SECTION("present headless");
    gfx_present();                                      // goes nowhere without a window, and that is fine
    gfx_clear(BG);
    gfx_present();

    TEST_SECTION("init again resizes");
    CHECK_EQ(gfx_init(32, 20), 0);
    CHECK_EQ(gfx_width(), 32);
    CHECK_EQ(gfx_height(), 20);
    CHECK_EQ(count(gfx_screen(), BG), 32 * 20);
}

// ---- lines and rectangles ----

static void lines(void)
{
    struct gfx_surface* s = canvas(40, 30);

    TEST_SECTION("horizontal, vertical, diagonal");
    gfx_line(2, 5, 20, 5, FG);
    CHECK_EQ(count(s, FG), 19);
    int row = 1;
    for (int x = 2; x <= 20; x++) if (pix(s, x, 5) != FG) row = 0;
    CHECK(row);
    gfx_clear(BG);
    gfx_line(7, 3, 7, 25, FG);
    CHECK_EQ(count(s, FG), 23);
    gfx_clear(BG);
    gfx_line(0, 0, 25, 25, FG);                         // exactly 45 degrees: (i, i)
    CHECK_EQ(count(s, FG), 26);
    int diag = 1;
    for (int i = 0; i <= 25; i++) if (pix(s, i, i) != FG) diag = 0;
    CHECK(diag);
    gfx_clear(BG);
    gfx_line(30, 2, 10, 22, FG);                        // the other diagonal: (30-i, 2+i)
    int anti = 1;
    for (int i = 0; i <= 20; i++) if (pix(s, 30 - i, 2 + i) != FG) anti = 0;
    CHECK(anti);
    CHECK_EQ(count(s, FG), 21);

    TEST_SECTION("a point, and both ends included");
    gfx_clear(BG);
    gfx_line(9, 9, 9, 9, FG);
    CHECK_EQ(count(s, FG), 1);
    CHECK_EQ((int)pix(s, 9, 9), (int)FG);
    gfx_clear(BG);
    gfx_line(3, 4, 30, 17, FG);
    CHECK_EQ((int)pix(s, 3, 4), (int)FG);
    CHECK_EQ((int)pix(s, 30, 17), (int)FG);

    TEST_SECTION("the same pixels in either direction, and one pixel per step");
    struct gfx_surface* t = gfx_surface_new(40, 30);
    int reversible = 1, steps = 1;
    int ends[8][4] = { {2, 3, 35, 9}, {35, 9, 2, 3}, {5, 25, 20, 2}, {1, 1, 38, 28}, {10, 4, 12, 27}, {0, 15, 39, 15}, {20, 0, 20, 29}, {6, 20, 33, 22} };
    for (int i = 0; i < 8; i++)
    {
        int* e = ends[i];
        gfx_set_target(s);
        gfx_clear(BG);
        gfx_line(e[0], e[1], e[2], e[3], FG);
        gfx_set_target(t);
        gfx_clear(BG);
        gfx_line(e[2], e[3], e[0], e[1], FG);
        if (!same_surface(s, t))
        {
            // Bresenham may break a tie differently going the other way: allow it, but not a different pixel count
            if (count(s, FG) != count(t, FG)) reversible = 0;
        }
        int dx = e[2] > e[0] ? e[2] - e[0] : e[0] - e[2];
        int dy = e[3] > e[1] ? e[3] - e[1] : e[1] - e[3];
        int longer = dx > dy ? dx : dy;
        if (count(s, FG) != longer + 1) steps = 0;
    }
    CHECK(reversible);
    CHECK(steps);                                       // max(|dx|, |dy|) + 1 pixels: no gaps, no doubled pixels
    gfx_surface_free(t);
    gfx_set_target(s);

    TEST_SECTION("a shallow line has one pixel per column");
    gfx_clear(BG);
    gfx_line(1, 3, 37, 14, FG);
    int per_col = 1;
    for (int x = 1; x <= 37; x++)
    {
        int n = 0;
        for (int y = 0; y < 30; y++) if (pix(s, x, y) == FG) n++;
        if (n != 1) per_col = 0;
    }
    CHECK(per_col);

    TEST_SECTION("spans");
    gfx_clear(BG);
    gfx_hline(5, 2, 10, FG);
    CHECK_EQ(count(s, FG), 10);
    CHECK_EQ((int)pix(s, 5, 2), (int)FG);
    CHECK_EQ((int)pix(s, 14, 2), (int)FG);
    CHECK_EQ((int)pix(s, 15, 2), (int)BG);
    gfx_hline(5, 3, 0, FG);                             // empty and negative lengths draw nothing
    gfx_hline(5, 3, -4, FG);
    gfx_vline(3, 3, 0, FG);
    gfx_vline(3, 3, -4, FG);
    CHECK_EQ(count(s, FG), 10);
    gfx_vline(20, 4, 12, FG);
    CHECK_EQ(count(s, FG), 22);
    CHECK_EQ((int)pix(s, 20, 15), (int)FG);
    CHECK_EQ((int)pix(s, 20, 16), (int)BG);

    TEST_SECTION("rectangles");
    gfx_clear(BG);
    gfx_rect(4, 5, 12, 8, FG);
    CHECK_EQ(count(s, FG), 2 * 12 + 2 * 8 - 4);
    CHECK_EQ((int)pix(s, 4, 5), (int)FG);
    CHECK_EQ((int)pix(s, 15, 12), (int)FG);
    CHECK_EQ((int)pix(s, 8, 8), (int)BG);                // hollow
    gfx_clear(BG);
    gfx_rect_fill(4, 5, 12, 8, FG);
    CHECK_EQ(count(s, FG), 96);
    CHECK_EQ((int)pix(s, 8, 8), (int)FG);
    CHECK_EQ((int)pix(s, 16, 8), (int)BG);
    gfx_clear(BG);
    gfx_rect_fill(5, 5, 0, 5, FG);                      // no width, no height, negative: nothing
    gfx_rect_fill(5, 5, 5, 0, FG);
    gfx_rect_fill(5, 5, -3, 5, FG);
    gfx_rect(5, 5, 0, 5, FG);
    CHECK_EQ(count(s, FG), 0);
    gfx_rect(5, 5, 1, 1, FG);                           // 1 x 1
    CHECK_EQ(count(s, FG), 1);
    gfx_clear(BG);
    gfx_rect(5, 5, 6, 1, FG);                           // one row high: just the line
    CHECK_EQ(count(s, FG), 6);
    gfx_clear(BG);
    gfx_rect(5, 5, 1, 6, FG);
    CHECK_EQ(count(s, FG), 6);
    gfx_clear(BG);
    gfx_rect(5, 5, 2, 2, FG);                           // 2 x 2: all four pixels once
    CHECK_EQ(count(s, FG), 4);
    done(s);
}

// ---- circles and triangles ----

static void circles(void)
{
    struct gfx_surface* s = canvas(41, 41);

    TEST_SECTION("circle outline");
    gfx_circle(20, 20, 10, FG);
    int sym_x = 1, sym_y = 1, sym_t = 1, band = 1;
    for (int y = 0; y < 41; y++)
        for (int x = 0; x < 41; x++)
        {
            unsigned int c = pix(s, x, y);
            if (c != pix(s, 40 - x, y)) sym_x = 0;
            if (c != pix(s, x, 40 - y)) sym_y = 0;
            if (c != pix(s, y, x)) sym_t = 0;            // symmetric about the diagonal too: the circle is centred at (20, 20)
            if (c == FG)
            {
                int d2 = (x - 20) * (x - 20) + (y - 20) * (y - 20);
                if (d2 < 81 || d2 > 121) band = 0;      // every outline pixel within a pixel of radius 10
            }
        }
    CHECK(sym_x);
    CHECK(sym_y);
    CHECK(sym_t);
    CHECK(band);
    CHECK_EQ((int)pix(s, 30, 20), (int)FG);              // the four extremes are on the circle
    CHECK_EQ((int)pix(s, 10, 20), (int)FG);
    CHECK_EQ((int)pix(s, 20, 30), (int)FG);
    CHECK_EQ((int)pix(s, 20, 10), (int)FG);
    CHECK_EQ((int)pix(s, 20, 20), (int)BG);              // and the middle is empty

    TEST_SECTION("filled circle");
    gfx_clear(BG);
    gfx_circle_fill(20, 20, 10, FG);
    int inside = count(s, FG);
    CHECK(inside > 300 && inside < 360);                // about pi * r^2 = 314, and a little more for the outer ring
    CHECK_EQ((int)pix(s, 20, 20), (int)FG);
    int all_inside = 1, filled = 1;
    for (int y = 0; y < 41; y++)
        for (int x = 0; x < 41; x++)
        {
            int d2 = (x - 20) * (x - 20) + (y - 20) * (y - 20);
            if (pix(s, x, y) == FG && d2 > 121) all_inside = 0;     // nothing beyond radius 11
            if (d2 <= 81 && pix(s, x, y) != FG) filled = 0;         // everything within radius 9
        }
    CHECK(all_inside);
    CHECK(filled);
    struct gfx_surface* o = gfx_surface_new(41, 41);    // the outline lies inside the fill
    gfx_set_target(o);
    gfx_circle(20, 20, 10, FG);
    int contained = 1;
    for (int i = 0; i < 41 * 41; i++)
        if (o->px[i] == FG && s->px[i] != FG) contained = 0;
    CHECK(contained);
    gfx_surface_free(o);
    gfx_set_target(s);

    TEST_SECTION("small and degenerate circles");
    gfx_clear(BG);
    gfx_circle(10, 10, 0, FG);
    CHECK_EQ(count(s, FG), 1);
    gfx_clear(BG);
    gfx_circle_fill(10, 10, 0, FG);
    CHECK_EQ(count(s, FG), 1);
    gfx_clear(BG);
    gfx_circle(10, 10, -3, FG);                         // a negative radius draws nothing
    gfx_circle_fill(10, 10, -3, FG);
    CHECK_EQ(count(s, FG), 0);
    gfx_circle_fill(20, 20, 1, FG);
    CHECK_EQ(count(s, FG), 5);                          // a plus sign
    gfx_clear(BG);
    gfx_circle_fill(20, 20, 2, FG);
    CHECK_EQ(count(s, FG), 21);
    done(s);
}

// inside test for a triangle, with `slack` pixels of tolerance measured as cross-product units
static int inside_tri(int px, int py, int x0, int y0, int x1, int y1, int x2, int y2, int slack)
{
    int d0 = (x1 - x0) * (py - y0) - (y1 - y0) * (px - x0);
    int d1 = (x2 - x1) * (py - y1) - (y2 - y1) * (px - x1);
    int d2 = (x0 - x2) * (py - y2) - (y0 - y2) * (px - x2);
    int neg = d0 < -slack || d1 < -slack || d2 < -slack;
    int pos = d0 > slack || d1 > slack || d2 > slack;
    return !(neg && pos);
}

static void triangles(void)
{
    struct gfx_surface* s = canvas(50, 40);

    TEST_SECTION("filled triangle geometry");
    gfx_triangle_fill(5, 5, 45, 12, 18, 35, FG);
    int n = count(s, FG);
    // area = |cross| / 2 = |(40)(30) - (7)(13)| / 2 = 554.5
    CHECK(n > 520 && n < 640);                          // the exact area plus about half the perimeter for the edge pixels
    int only_inside = 1, core_filled = 1;
    for (int y = 0; y < 40; y++)
        for (int x = 0; x < 50; x++)
        {
            int loose = inside_tri(x, y, 5, 5, 45, 12, 18, 35, 60);        // a pixel or so beyond the edges
            if (pix(s, x, y) == FG && !loose) only_inside = 0;
            // a pixel well inside (its cross products all clearly one sign) must be filled
            int d0 = (45 - 5) * (y - 5) - (12 - 5) * (x - 5);
            int d1 = (18 - 45) * (y - 12) - (35 - 12) * (x - 45);
            int d2 = (5 - 18) * (y - 35) - (5 - 35) * (x - 18);
            int margin = 60;
            int strictly = (d0 > margin && d1 > margin && d2 > margin) || (d0 < -margin && d1 < -margin && d2 < -margin);
            if (strictly && pix(s, x, y) != FG) core_filled = 0;
        }
    CHECK(only_inside);
    CHECK(core_filled);

    TEST_SECTION("the order of the corners does not matter");
    struct gfx_surface* o = gfx_surface_new(50, 40);
    int order_ok = 1;
    int p[6][6] = { {5, 5, 45, 12, 18, 35}, {45, 12, 5, 5, 18, 35}, {18, 35, 5, 5, 45, 12},
                    {5, 5, 18, 35, 45, 12}, {18, 35, 45, 12, 5, 5}, {45, 12, 18, 35, 5, 5} };
    for (int i = 1; i < 6; i++)
    {
        gfx_set_target(o);
        gfx_clear(BG);
        gfx_triangle_fill(p[i][0], p[i][1], p[i][2], p[i][3], p[i][4], p[i][5], FG);
        // the same shape to within the rounding of an edge that is walked from the other end
        int differ = 0;
        for (int k = 0; k < 50 * 40; k++)
            if ((o->px[k] == FG) != (s->px[k] == FG)) differ++;
        if (differ > 40) order_ok = 0;
    }
    CHECK(order_ok);
    gfx_surface_free(o);
    gfx_set_target(s);

    TEST_SECTION("flat and degenerate triangles");
    gfx_clear(BG);
    gfx_triangle_fill(10, 10, 30, 10, 20, 25, FG);      // flat top
    CHECK_EQ((int)pix(s, 10, 10), (int)FG);
    CHECK_EQ((int)pix(s, 30, 10), (int)FG);
    CHECK_EQ((int)pix(s, 20, 25), (int)FG);              // the bottom tip
    CHECK_EQ((int)pix(s, 20, 15), (int)FG);
    CHECK_EQ((int)pix(s, 10, 25), (int)BG);
    CHECK(count(s, FG) > 160 && count(s, FG) < 200);    // 20 * 15 / 2 = 150 plus the edge pixels
    gfx_clear(BG);
    gfx_triangle_fill(20, 5, 8, 30, 32, 30, FG);        // flat bottom
    CHECK_EQ((int)pix(s, 20, 5), (int)FG);
    CHECK_EQ((int)pix(s, 8, 30), (int)FG);
    CHECK_EQ((int)pix(s, 32, 30), (int)FG);
    CHECK_EQ((int)pix(s, 20, 20), (int)FG);
    CHECK_EQ((int)pix(s, 8, 5), (int)BG);
    gfx_clear(BG);
    gfx_triangle_fill(5, 7, 30, 7, 12, 7, FG);          // all on one row: a span from the leftmost to the rightmost
    CHECK_EQ(count(s, FG), 26);
    gfx_clear(BG);
    gfx_triangle_fill(9, 9, 9, 9, 9, 9, FG);            // a point
    CHECK_EQ(count(s, FG), 1);
    gfx_clear(BG);
    gfx_triangle_fill(5, 5, 5, 20, 5, 35, FG);          // a vertical line
    CHECK_EQ(count(s, FG), 31);

    TEST_SECTION("triangle outline");
    gfx_clear(BG);
    gfx_triangle(5, 5, 40, 5, 22, 30, FG);
    CHECK_EQ((int)pix(s, 5, 5), (int)FG);
    CHECK_EQ((int)pix(s, 40, 5), (int)FG);
    CHECK_EQ((int)pix(s, 22, 30), (int)FG);
    CHECK_EQ((int)pix(s, 22, 10), (int)BG);              // hollow
    CHECK_EQ((int)pix(s, 20, 5), (int)FG);
    done(s);
}

// ---- clipping ----

// Draws big shapes with the clip set to (10,10)-(29,29) on a 40x40 surface whose every pixel is GUARD, and
// checks that only pixels inside the clip changed.
static int untouched_outside(const struct gfx_surface* s)
{
    for (int y = 0; y < s->h; y++)
        for (int x = 0; x < s->w; x++)
        {
            int inside = x >= 10 && x < 30 && y >= 10 && y < 30;
            if (!inside && pix(s, x, y) != GUARD) return 0;
        }
    return 1;
}

static void clipping(void)
{
    struct gfx_surface* s = canvas(40, 40);

    TEST_SECTION("a clip rectangle");
    gfx_clear(GUARD);
    gfx_set_clip(10, 10, 20, 20);
    int cx, cy, cw, ch;
    gfx_get_clip(&cx, &cy, &cw, &ch);
    CHECK(cx == 10 && cy == 10 && cw == 20 && ch == 20);
    gfx_line(-100, -100, 200, 200, FG);
    CHECK(untouched_outside(s));
    CHECK_EQ((int)pix(s, 15, 15), (int)FG);              // the part inside is drawn
    CHECK_EQ((int)pix(s, 5, 5), (int)GUARD);
    gfx_line(-500, 20, 500, 20, 0xAAAAAA);
    gfx_line(20, -500, 20, 500, 0xAAAAAA);
    gfx_hline(-1000000, 12, 2000000, 0xBBBBBB);         // enormous spans still land inside only
    gfx_vline(12, -1000000, 2000000, 0xBBBBBB);
    gfx_rect(-5, -5, 100, 100, 0xCCCCCC);
    gfx_rect_fill(0, 0, 40, 5, 0xDDDDDD);               // entirely above the clip
    gfx_rect_fill(25, 25, 100, 100, 0xEEEEEE);          // straddling a corner
    gfx_circle(20, 20, 50, 0x111111);
    gfx_circle_fill(0, 0, 25, 0x222222);
    gfx_circle_fill(39, 39, 25, 0x333333);
    gfx_triangle_fill(-30, -30, 80, 10, 10, 90, 0x444444);
    gfx_triangle(-30, 50, 80, -10, 100, 100, 0x555555);
    gfx_pixel(9, 20, 0x666666);
    gfx_pixel(30, 20, 0x666666);
    gfx_pixel(20, 9, 0x666666);
    gfx_pixel(20, 30, 0x666666);
    CHECK(untouched_outside(s));                        // through all of that, not one pixel outside
    CHECK(count(s, GUARD) < 1600 - 100);                // and there was drawing inside the clip

    TEST_SECTION("blits and text obey it too");
    struct gfx_surface* src = gfx_surface_new(30, 30);
    for (int i = 0; i < 900; i++) src->px[i] = 0x777777;
    gfx_blit(src, 0, 0, 30, 30, 0, 0);                  // mostly outside
    gfx_blit(src, 0, 0, 30, 30, 20, 20);
    gfx_blit(src, 0, 0, 30, 30, -20, -20);
    gfx_blit_key(src, 0, 0, 30, 30, 5, 5, 0x000000);
    gfx_blit_scaled(src, -10, -10, 3);
    font_text(NULL, 0, 0, "clipped text is fine", 0x888888, 3);
    font_text(NULL, 12, 26, "AB", 0x888888, 4);
    CHECK(untouched_outside(s));
    gfx_surface_free(src);

    TEST_SECTION("an empty clip draws nothing");
    gfx_clear(GUARD);
    gfx_set_clip(10, 10, 0, 5);
    gfx_rect_fill(0, 0, 40, 40, FG);
    gfx_line(0, 0, 39, 39, FG);
    gfx_set_clip(10, 10, -5, 5);
    gfx_rect_fill(0, 0, 40, 40, FG);
    gfx_set_clip(100, 100, 5, 5);                       // wholly outside the surface
    gfx_rect_fill(0, 0, 40, 40, FG);
    gfx_pixel(20, 20, FG);
    CHECK_EQ(count(s, GUARD), 1600);

    TEST_SECTION("a clip larger than the surface is limited to it");
    gfx_set_clip(-10, -10, 100, 100);
    gfx_get_clip(&cx, &cy, &cw, &ch);
    CHECK(cx == 0 && cy == 0 && cw == 40 && ch == 40);
    gfx_rect_fill(-5, -5, 100, 100, FG);
    CHECK_EQ(count(s, FG), 1600);

    TEST_SECTION("reset");
    gfx_set_clip(10, 10, 5, 5);
    gfx_reset_clip();
    gfx_get_clip(&cx, &cy, &cw, &ch);
    CHECK(cx == 0 && cy == 0 && cw == 40 && ch == 40);
    gfx_clear(BG);
    gfx_set_clip(10, 10, 5, 5);
    gfx_clear(FG);                                      // gfx_clear ignores the clip
    CHECK_EQ(count(s, FG), 1600);
    done(s);
}

// ---- surfaces and blits ----

static void surfaces(void)
{
    TEST_SECTION("creating and freeing");
    CHECK(gfx_surface_new(0, 5) == NULL);
    CHECK(gfx_surface_new(5, 0) == NULL);
    CHECK(gfx_surface_new(-1, 5) == NULL);
    struct gfx_surface* a = gfx_surface_new(16, 12);
    CHECK(a != NULL);
    CHECK(a->w == 16 && a->h == 12);
    CHECK_EQ(count(a, BG), 16 * 12);                    // zeroed
    gfx_surface_free(NULL);                             // safe

    TEST_SECTION("drawing goes to the target, and only there");
    struct gfx_surface* scr = gfx_screen();
    gfx_set_target(NULL);
    gfx_clear(BG);
    gfx_set_target(a);
    CHECK(gfx_target() == a);
    CHECK_EQ(gfx_width(), 16);
    CHECK_EQ(gfx_height(), 12);
    gfx_rect_fill(0, 0, 16, 12, FG);
    CHECK_EQ(count(a, FG), 16 * 12);
    CHECK_EQ(count(scr, FG), 0);                        // the screen was not touched
    gfx_set_target(NULL);
    CHECK(gfx_target() == scr);
    CHECK_EQ(gfx_width(), 32);

    TEST_SECTION("blit copies exactly");
    for (int y = 0; y < 12; y++)
        for (int x = 0; x < 16; x++)
            a->px[y * 16 + x] = 0x010000 * y + x + 1;   // every pixel different
    struct gfx_surface* b = gfx_surface_new(30, 20);
    gfx_set_target(b);
    gfx_blit(a, 0, 0, 16, 12, 5, 4);
    int copied = 1;
    for (int y = 0; y < 12; y++)
        for (int x = 0; x < 16; x++)
            if (pix(b, 5 + x, 4 + y) != pix(a, x, y)) copied = 0;
    CHECK(copied);
    CHECK_EQ(count(b, BG), 30 * 20 - 16 * 12);          // and nothing else changed

    gfx_clear(BG);
    gfx_blit(a, 4, 3, 6, 5, 20, 10);                    // a sub-rectangle
    CHECK_EQ((int)pix(b, 20, 10), (int)pix(a, 4, 3));
    CHECK_EQ((int)pix(b, 25, 14), (int)pix(a, 9, 7));
    CHECK_EQ((int)pix(b, 26, 10), (int)BG);
    CHECK_EQ(count(b, BG), 30 * 20 - 30);

    TEST_SECTION("blit partly outside");
    gfx_clear(BG);
    gfx_blit(a, 0, 0, 16, 12, -4, -3);                  // off the top left: the visible part only
    CHECK_EQ((int)pix(b, 0, 0), (int)pix(a, 4, 3));
    CHECK_EQ((int)pix(b, 11, 8), (int)pix(a, 15, 11));
    CHECK_EQ(count(b, BG), 30 * 20 - 12 * 9);
    gfx_clear(BG);
    gfx_blit(a, 0, 0, 16, 12, 22, 15);                  // off the bottom right
    CHECK_EQ((int)pix(b, 22, 15), (int)pix(a, 0, 0));
    CHECK_EQ((int)pix(b, 29, 19), (int)pix(a, 7, 4));
    CHECK_EQ(count(b, BG), 30 * 20 - 8 * 5);
    gfx_clear(BG);
    gfx_blit(a, -5, -5, 30, 30, 2, 2);                  // a source rectangle bigger than the source
    CHECK_EQ((int)pix(b, 7, 7), (int)pix(a, 0, 0));
    gfx_clear(BG);
    gfx_blit(a, 0, 0, 16, 12, 100, 100);                // nothing to draw
    gfx_blit(a, 0, 0, 0, 12, 3, 3);
    gfx_blit(a, 0, 0, 16, -1, 3, 3);
    gfx_blit(a, 40, 40, 5, 5, 3, 3);
    gfx_blit(NULL, 0, 0, 5, 5, 3, 3);
    CHECK_EQ(count(b, BG), 30 * 20);

    TEST_SECTION("a surface blitted onto itself");
    gfx_clear(BG);
    for (int y = 0; y < 20; y++)
        for (int x = 0; x < 30; x++)
            b->px[y * 30 + x] = 0x0100 * y + x + 1;
    struct gfx_surface* copy = gfx_surface_new(30, 20);
    for (int i = 0; i < 600; i++) copy->px[i] = b->px[i];
    gfx_blit(b, 0, 0, 20, 15, 3, 2);                    // the regions overlap: it must behave as if copied through a buffer
    int scrolled = 1;
    for (int y = 0; y < 15; y++)
        for (int x = 0; x < 20; x++)
            if (pix(b, 3 + x, 2 + y) != pix(copy, x, y)) scrolled = 0;
    CHECK(scrolled);
    gfx_surface_free(copy);

    TEST_SECTION("a colour key");
    gfx_clear(BG);
    struct gfx_surface* k = gfx_surface_new(4, 4);
    for (int i = 0; i < 16; i++) k->px[i] = (i % 2) ? 0xFF00FF : 0x00FF00;      // a checkerboard of key and colour
    gfx_rect_fill(0, 0, 30, 20, 0x999999);
    gfx_blit_key(k, 0, 0, 4, 4, 10, 10, 0xFF00FF);
    int keyed = 1;
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
        {
            unsigned int got = pix(b, 10 + x, 10 + y);
            unsigned int want = k->px[y * 4 + x] == 0xFF00FF ? 0x999999 : k->px[y * 4 + x];
            if (got != want) keyed = 0;
        }
    CHECK(keyed);
    gfx_surface_free(k);

    TEST_SECTION("scaled");
    struct gfx_surface* tiny = gfx_surface_new(2, 2);
    tiny->px[0] = 0x111111; tiny->px[1] = 0x222222; tiny->px[2] = 0x333333; tiny->px[3] = 0x444444;
    gfx_clear(BG);
    gfx_blit_scaled(tiny, 4, 3, 3);
    CHECK_EQ((int)pix(b, 4, 3), 0x111111);
    CHECK_EQ((int)pix(b, 6, 5), 0x111111);               // a 3 x 3 block
    CHECK_EQ((int)pix(b, 7, 3), 0x222222);
    CHECK_EQ((int)pix(b, 4, 6), 0x333333);
    CHECK_EQ((int)pix(b, 9, 8), 0x444444);
    CHECK_EQ((int)pix(b, 10, 8), (int)BG);
    CHECK_EQ(count(b, BG), 30 * 20 - 36);
    gfx_clear(BG);
    gfx_blit_scaled(tiny, 4, 3, 0);                     // not a scale
    gfx_blit_scaled(NULL, 4, 3, 2);
    CHECK_EQ(count(b, BG), 30 * 20);
    gfx_surface_free(tiny);

    TEST_SECTION("freeing the target");
    gfx_surface_free(b);                                // b is the target: the screen takes over
    CHECK(gfx_target() == scr);
    gfx_surface_free(a);
}

// ---- text ----

static void text(void)
{
    struct gfx_surface* s = canvas(64, 32);

    TEST_SECTION("the glyph table");
    int blank_edges = 1, all_drawn = 1;
    for (int c = 33; c < 127; c++)
    {
        int bits = 0;
        for (int row = 0; row < 8; row++)
        {
            bits |= font8x8[c][row];
            if (font8x8[c][row] & 0xE0) blank_edges = 0;    // columns 5..7 are the gap between characters
        }
        if (font8x8[c][7] != 0) blank_edges = 0;             // and row 7
        if (bits == 0) all_drawn = 0;                        // every printable character but the space has ink
    }
    CHECK(blank_edges);
    CHECK(all_drawn);
    int blank = 1;
    for (int row = 0; row < 8; row++) if (font8x8[' '][row] != 0) blank = 0;
    CHECK(blank);
    int distinct = 1;
    for (int a = 32; a < 127; a++)
        for (int b = a + 1; b < 127; b++)
        {
            int same = 1;
            for (int row = 0; row < 8; row++) if (font8x8[a][row] != font8x8[b][row]) same = 0;
            if (same) distinct = 0;
        }
    CHECK(distinct);
    CHECK_EQ((int)font8x8['A'][3], 0x1F);                // the crossbar of the A: five pixels
    CHECK_EQ((int)font8x8['I'][0], 0x0E);
    CHECK_EQ((int)font8x8['-'][3], 0x1F);

    TEST_SECTION("one character");
    gfx_clear(BG);
    font_char(s, 3, 2, 'A', FG, 1);
    int match = 1, ink = 0;
    for (int row = 0; row < 8; row++)
        for (int col = 0; col < 8; col++)
        {
            int want = (font8x8['A'][row] >> col) & 1;
            if ((pix(s, 3 + col, 2 + row) == FG) != want) match = 0;
            ink += want;
        }
    CHECK(match);
    CHECK_EQ(count(s, FG), ink);                        // nothing outside the 8 x 8 cell
    gfx_clear(BG);
    font_char(s, 0, 0, ' ', FG, 1);
    CHECK_EQ(count(s, FG), 0);
    font_char(s, 0, 0, (char)0x85, FG, 1);              // a control (C1): blank; 0xA0 and up are Latin-1
    font_char(s, 0, 0, (char)0, FG, 1);
    font_char(s, 0, 0, 'A', FG, 0);                     // not a scale
    font_char(s, 0, 0, 'A', FG, -2);
    CHECK_EQ(count(s, FG), 0);

    TEST_SECTION("scaling");
    gfx_clear(BG);
    font_char(s, 4, 3, 'A', FG, 3);
    int scaled = 1;
    for (int row = 0; row < 8; row++)
        for (int col = 0; col < 8; col++)
        {
            int want = (font8x8['A'][row] >> col) & 1;
            for (int dy = 0; dy < 3; dy++)
                for (int dx = 0; dx < 3; dx++)
                    if ((pix(s, 4 + col * 3 + dx, 3 + row * 3 + dy) == FG) != want) scaled = 0;
        }
    CHECK(scaled);
    CHECK_EQ(count(s, FG), ink * 9);

    TEST_SECTION("strings");
    gfx_clear(BG);
    font_text(s, 0, 0, "Hi", FG, 1);
    struct gfx_surface* o = gfx_surface_new(64, 32);
    gfx_set_target(o);
    font_char(o, 0, 0, 'H', FG, 1);
    font_char(o, 8, 0, 'i', FG, 1);                     // the second character starts 8 pixels on
    CHECK(same_surface(s, o));
    gfx_clear(BG);
    font_text(o, 2, 5, "ab\ncd", FG, 1);                // '\n' returns to x and goes down 8
    gfx_set_target(s);
    gfx_clear(BG);
    font_char(s, 2, 5, 'a', FG, 1);
    font_char(s, 10, 5, 'b', FG, 1);
    font_char(s, 2, 13, 'c', FG, 1);
    font_char(s, 10, 13, 'd', FG, 1);
    CHECK(same_surface(s, o));

    TEST_SECTION("printf");
    gfx_clear(BG);
    font_printf(s, 1, 1, FG, 1, "%d+%d=%s", 2, 3, "five");
    gfx_set_target(o);
    gfx_clear(BG);
    font_text(o, 1, 1, "2+3=five", FG, 1);
    CHECK(same_surface(s, o));
    gfx_set_target(s);
    gfx_clear(BG);
    font_printf(s, 0, 0, FG, 1, "%s", "");
    CHECK_EQ(count(s, FG), 0);

    TEST_SECTION("measuring");
    CHECK_EQ(font_text_width("", 1), 0);
    CHECK_EQ(font_text_height("", 1), 8);
    CHECK_EQ(font_text_width("Hello", 1), 40);
    CHECK_EQ(font_text_width("Hi", 2), 32);
    CHECK_EQ(font_text_width("ab\ncdef\ng", 1), 32);    // the longest line
    CHECK_EQ(font_text_height("ab\ncdef\ng", 1), 24);
    CHECK_EQ(font_text_height("ab\ncdef\ng", 2), 48);
    CHECK_EQ(font_text_width("\n\n", 3), 0);

    TEST_SECTION("text into another surface keeps the caller's target and clip");
    gfx_set_target(s);
    gfx_set_clip(4, 6, 20, 10);
    font_text(o, 0, 0, "zz", FG, 1);
    CHECK(gfx_target() == s);
    int cx, cy, cw, ch;
    gfx_get_clip(&cx, &cy, &cw, &ch);
    CHECK(cx == 4 && cy == 6 && cw == 20 && ch == 10);
    gfx_reset_clip();
    gfx_surface_free(o);
    done(s);
}

// ---- sprites, animation, tile maps ----

static const unsigned int spx[12] = {          // 4 x 3: '.' is the key (0), '#' a colour
    0, 1, 2, 0,
    3, 4, 5, 6,
    0, 7, 8, 0
};
static const struct sprite spr = { 4, 3, spx, 0 };

static const unsigned int solid_px[4] = { 5, 5, 5, 5 };
static const struct sprite solid = { 2, 2, solid_px, 0xFFFFFF };
static const unsigned int dark_px[4] = { 9, 9, 9, 9 };
static const struct sprite dark = { 2, 2, dark_px, 0xFFFFFF };

static void sprites(void)
{
    struct gfx_surface* s = canvas(20, 12);

    TEST_SECTION("sprite");
    gfx_clear(50);
    sprite_draw(&spr, 3, 2);
    int right = 1;
    for (int y = 0; y < 3; y++)
        for (int x = 0; x < 4; x++)
        {
            unsigned int want = spx[y * 4 + x] == 0 ? 50 : spx[y * 4 + x];
            if (pix(s, 3 + x, 2 + y) != want) right = 0;
        }
    CHECK(right);
    CHECK_EQ(count(s, 50), 20 * 12 - 8);                // the key pixels left the background showing
    sprite_draw(NULL, 0, 0);                            // nothing to draw

    TEST_SECTION("flipped");
    gfx_clear(50);
    sprite_draw_flip(&spr, 3, 2, 1, 0);
    CHECK_EQ((int)pix(s, 3, 3), 6);                      // row 1 read right to left: 6 5 4 3
    CHECK_EQ((int)pix(s, 6, 3), 3);
    CHECK_EQ((int)pix(s, 3, 2), 50);                     // the key stayed transparent
    CHECK_EQ((int)pix(s, 5, 2), 1);
    gfx_clear(50);
    sprite_draw_flip(&spr, 3, 2, 0, 1);
    CHECK_EQ((int)pix(s, 4, 2), 7);                      // the last row first
    CHECK_EQ((int)pix(s, 3, 3), 3);
    CHECK_EQ((int)pix(s, 4, 4), 1);
    gfx_clear(50);
    sprite_draw_flip(&spr, 3, 2, 1, 1);                 // rotated half a turn
    CHECK_EQ((int)pix(s, 4, 2), 8);
    CHECK_EQ((int)pix(s, 5, 2), 7);
    CHECK_EQ((int)pix(s, 6, 4), 50);
    gfx_clear(50);
    sprite_draw_flip(&spr, 3, 2, 0, 0);                 // no flip is a plain draw
    CHECK_EQ((int)pix(s, 4, 2), 1);
    gfx_clear(50);
    sprite_draw_flip(&spr, 18, 10, 1, 1);               // clipped at the corner
    CHECK_EQ(count(s, 50), 20 * 12 - 3);
    gfx_clear(50);
    sprite_draw_scaled(&spr, 2, 1, 2);
    CHECK_EQ((int)pix(s, 2, 1), 50);                     // the key still skips, scaled
    CHECK_EQ((int)pix(s, 4, 1), 1);
    CHECK_EQ((int)pix(s, 5, 2), 1);
    CHECK_EQ((int)pix(s, 4, 3), 4);
    CHECK_EQ(count(s, 50), 20 * 12 - 8 * 4);

    TEST_SECTION("animation");
    const struct sprite* frames[3] = { &spr, &solid, &dark };
    struct anim a;
    anim_init(&a, frames, 3, 4);
    CHECK(anim_frame(&a) == &spr);
    anim_update(&a, 3);
    CHECK(anim_frame(&a) == &spr);                      // not yet
    anim_update(&a, 1);
    CHECK(anim_frame(&a) == &solid);
    anim_update(&a, 4);
    CHECK(anim_frame(&a) == &dark);
    anim_update(&a, 4);
    CHECK(anim_frame(&a) == &spr);                      // wraps around
    anim_update(&a, 9);                                 // more than two frames at once: 9 ticks = 2 frames and 1 left over
    CHECK(anim_frame(&a) == &dark);
    CHECK_EQ(a.elapsed, 1);
    anim_update(&a, 0);
    anim_update(&a, -5);                                // time does not run backwards
    CHECK(anim_frame(&a) == &dark);
    anim_reset(&a);
    CHECK(anim_frame(&a) == &spr);
    struct anim frozen;
    anim_init(&frozen, frames, 3, 0);
    anim_update(&frozen, 100);
    CHECK(anim_frame(&frozen) == &spr);                 // no frame time: it stays on the first
    struct anim empty;
    anim_init(&empty, frames, 0, 5);
    anim_update(&empty, 10);
    CHECK(anim_frame(&empty) == NULL);

    TEST_SECTION("rectangles");
    struct rect r1 = { 10, 10, 20, 10 };
    struct rect r2 = { 25, 15, 20, 20 };
    struct rect r3 = { 30, 10, 5, 5 };                  // touches r1's right edge without sharing a pixel
    struct rect r4 = { 12, 12, 3, 3 };                  // inside r1
    struct rect r5 = { 10, 20, 20, 5 };                 // touches r1's bottom edge
    struct rect z = { 15, 15, 0, 5 };
    CHECK(rect_overlap(&r1, &r2));
    CHECK(rect_overlap(&r2, &r1));                      // symmetric
    CHECK(!rect_overlap(&r1, &r3));
    CHECK(rect_overlap(&r1, &r4));
    CHECK(rect_overlap(&r4, &r1));
    CHECK(!rect_overlap(&r1, &r5));
    CHECK(rect_overlap(&r1, &r1));
    CHECK(!rect_overlap(&r1, &z));                      // an empty rectangle overlaps nothing
    CHECK(rect_contains(&r1, 10, 10));
    CHECK(rect_contains(&r1, 29, 19));
    CHECK(!rect_contains(&r1, 30, 19));
    CHECK(!rect_contains(&r1, 29, 20));
    CHECK(!rect_contains(&r1, 9, 10));
    CHECK(!rect_contains(&z, 15, 15));

    done(s);
}

static const unsigned char map_tiles[12] = {   // 4 columns x 3 rows
    0, 1, 1, 0,
    1, 2, 2, 1,
    0, 1, 1, 0
};
static const struct sprite* tileset[3] = { NULL, &solid, &dark };
static const struct tilemap map = { 2, 2, 4, 3, map_tiles, tileset };

static void tilemaps(void)
{
    struct gfx_surface* s = canvas(8, 6);

    TEST_SECTION("looking tiles up");
    CHECK_EQ(tilemap_tile(&map, 0, 0), 0);
    CHECK_EQ(tilemap_tile(&map, 1, 1), 2);
    CHECK_EQ(tilemap_tile(&map, 3, 1), 1);
    CHECK_EQ(tilemap_tile(&map, 4, 0), -1);
    CHECK_EQ(tilemap_tile(&map, 0, 3), -1);
    CHECK_EQ(tilemap_tile(&map, -1, 0), -1);
    CHECK_EQ(tilemap_at(&map, 0, 0), 0);
    CHECK_EQ(tilemap_at(&map, 1, 1), 0);                // still inside the first 2 x 2 tile
    CHECK_EQ(tilemap_at(&map, 2, 2), 2);                // the next tile down and across
    CHECK_EQ(tilemap_at(&map, 7, 3), 1);
    CHECK_EQ(tilemap_at(&map, 8, 0), -1);               // past the right edge of the map
    CHECK_EQ(tilemap_at(&map, 0, 6), -1);
    CHECK_EQ(tilemap_at(&map, -1, 2), -1);

    TEST_SECTION("drawing the whole map");
    gfx_clear(50);
    tilemap_draw(&map, 0, 0);
    int whole = 1;
    for (int y = 0; y < 6; y++)
        for (int x = 0; x < 8; x++)
        {
            int id = map_tiles[(y / 2) * 4 + (x / 2)];
            unsigned int want = id == 0 ? 50 : (id == 1 ? 5 : 9);      // tile 0 is NULL: nothing drawn
            if (pix(s, x, y) != want) whole = 0;
        }
    CHECK(whole);

    TEST_SECTION("scrolled by a camera");
    gfx_clear(50);
    tilemap_draw(&map, 3, 1);                           // the map moves 3 left and 1 up
    int shifted = 1;
    for (int y = 0; y < 6; y++)
        for (int x = 0; x < 8; x++)
        {
            int id = tilemap_at(&map, x + 3, y + 1);
            unsigned int want = (id <= 0) ? 50 : (id == 1 ? 5 : 9);
            if (pix(s, x, y) != want) shifted = 0;
        }
    CHECK(shifted);
    gfx_clear(50);
    tilemap_draw(&map, -3, -2);                         // the camera before the map: it appears further in
    int before = 1;
    for (int y = 0; y < 6; y++)
        for (int x = 0; x < 8; x++)
        {
            int id = tilemap_at(&map, x - 3, y - 2);
            unsigned int want = (id <= 0) ? 50 : (id == 1 ? 5 : 9);
            if (pix(s, x, y) != want) before = 0;
        }
    CHECK(before);
    gfx_clear(50);
    tilemap_draw(&map, 100, 100);                       // the camera far past the map, and far before it
    tilemap_draw(&map, -100, -100);
    tilemap_draw(&map, 8, 0);
    CHECK_EQ(count(s, 50), 8 * 6);
    done(s);
}

int main(void)
{
    colours();
    screen();
    lines();
    circles();
    triangles();
    clipping();
    surfaces();
    text();
    sprites();
    tilemaps();

    TEST_SECTION("shutdown");
    gfx_shutdown();
    CHECK(gfx_screen() == NULL);
    CHECK(gfx_target() == NULL);
    gfx_pixel(0, 0, FG);                                // after shutdown a draw does nothing
    gfx_present();
    gfx_shutdown();                                     // twice is fine
    return test_summary();
}
