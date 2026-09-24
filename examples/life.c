// Conway's Game of Life on the pixel display: a 40 x 25 torus of 8-pixel cells.
//
//   Space  pause / resume        S  one step (while paused)
//   R      a random soup         C  clear
//   Esc    quit (closing the window also ends it)
//
// It starts from the R-pentomino, which grows for over a thousand generations. The frame loop is paced by
// sleeping (game_pace_ms), so the host rests between frames.
//
// Built with -DDEMO_FRAMES=100 it plays itself for that many generations without a window or a pause,
// printing the population every ten: that is what examples/expected/life.expected records.
#include "stdio.h"
#include "string.h"
#include "ceres/gfx.h"
#include "ceres/font.h"
#include "ceres/game.h"
#include "ceres/input.h"
#include "ceres/rand.h"

#define COLS 40
#define ROWS 25
#define CELL 8

static unsigned char cells[COLS * ROWS];
static unsigned char next_cells[COLS * ROWS];
static unsigned int generation;
static struct rng rng;

static int alive(int x, int y)              // the grid wraps around at every edge
{
    x = (x + COLS) % COLS;
    y = (y + ROWS) % ROWS;
    return cells[y * COLS + x];
}

static int population(void)
{
    int n = 0;
    for (int i = 0; i < COLS * ROWS; i++)
        n += cells[i];
    return n;
}

static void clear_grid(void)
{
    memset(cells, 0, sizeof cells);
    generation = 0;
}

static void seed_r_pentomino(void)
{
    clear_grid();
    int cx = COLS / 2, cy = ROWS / 2;
    cells[(cy + 0) * COLS + (cx + 1)] = 1;
    cells[(cy + 0) * COLS + (cx + 2)] = 1;
    cells[(cy + 1) * COLS + (cx + 0)] = 1;
    cells[(cy + 1) * COLS + (cx + 1)] = 1;
    cells[(cy + 2) * COLS + (cx + 1)] = 1;
}

static void seed_soup(void)
{
    clear_grid();
    for (int i = 0; i < COLS * ROWS; i++)
        cells[i] = (unsigned char)rng_chance(&rng, 30);
}

static void step(void)
{
    for (int y = 0; y < ROWS; y++)
        for (int x = 0; x < COLS; x++)
        {
            int n = alive(x - 1, y - 1) + alive(x, y - 1) + alive(x + 1, y - 1) +
                    alive(x - 1, y)                         + alive(x + 1, y) +
                    alive(x - 1, y + 1) + alive(x, y + 1) + alive(x + 1, y + 1);
            int now = cells[y * COLS + x];
            next_cells[y * COLS + x] = (unsigned char)(n == 3 || (now && n == 2));
        }
    memcpy(cells, next_cells, sizeof cells);
    generation++;
}

static void draw(void)
{
    gfx_clear(RGB(12, 16, 24));
    for (int y = 0; y < ROWS; y++)
        for (int x = 0; x < COLS; x++)
            if (cells[y * COLS + x])
                gfx_rect_fill(x * CELL + 1, y * CELL + 1, CELL - 2, CELL - 2, color_hsv(100 + (x + y) * 3, 200, 230));
    gfx_rect_fill(0, 0, 130, 10, RGB(0, 0, 0));
    font_printf(NULL, 1, 1, RGB(200, 200, 200), 1, "gen %u  pop %d", generation, population());
    gfx_present();
}

int main(void)
{
    if (gfx_init(COLS * CELL, ROWS * CELL) != 0)
    {
        puts("life: no display");
        return 1;
    }
    rng_seed(&rng, 2026);
    seed_r_pentomino();

    struct game g;
#ifdef DEMO_FRAMES
    game_init(&g, 0);                       // no pacing: run as fast as the machine goes
    printf("gen 0 pop %d\n", population());
    for (int f = 0; f < DEMO_FRAMES; f++)
    {
        game_frame_begin(&g);
        step();
        draw();
        game_frame_end(&g);
        if (generation % 10 == 0)
            printf("gen %u pop %d\n", generation, population());
    }
    return 0;
#else
    game_init(&g, 0);
    game_pace_ms(&g, 16);                   // about 60 frames a second
    input_terminal_fallback(1);             // in a plain terminal the letters below arrive as keys too
    int paused = 0, frames = 0;
    while (g.running)
    {
        game_frame_begin(&g);
        if (key_pressed(KEY_ESCAPE))
            game_quit(&g);
        if (key_pressed(KEY_SPACE))
            paused = !paused;
        if (key_pressed(KEY_R))
            seed_soup();
        if (key_pressed(KEY_C))
            clear_grid();
        if (paused && key_pressed(KEY_S))
            step();
        if (!paused && ++frames % 5 == 0)   // twelve generations a second
            step();
        draw();
        game_frame_end(&g);
    }
    return 0;
#endif
}
