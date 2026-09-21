// USE: irq
// Snake on the pixel display: a 40 x 25 grid of 8-pixel cells.
//
//   arrows / W A S D / the gamepad's d-pad   steer
//   Enter                                    play again after a crash
//   Esc                                      quit (closing the window also ends it)
//
// Eating the food grows the snake and speeds the game up a little. The frame loop is paced by halting
// (game_pace_ms), which is why it links the irq module.
//
// Built with -DDEMO_FRAMES=1200 a bot plays instead of you - no window, no pacing, a move per frame - and
// the program prints how it did: that is what examples/expected/snake.expected records.
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
#define MAXLEN (COLS * ROWS)

static int body_x[MAXLEN];                  // the snake as a ring of cells, oldest first
static int body_y[MAXLEN];
static unsigned char occupied[COLS * ROWS]; // which cells the body covers, for a constant-time collision test
static int head;                            // index of the head in the ring
static int length;
static int dir_x, dir_y;                    // the way it is going
static int want_x, want_y;                  // the way the player last asked for
static int food_x, food_y;
static int score;
static int crashed;
static struct rng rng;

static int tail_index(void)
{
    return (head - length + 1 + MAXLEN) % MAXLEN;
}

static void place_food(void)
{
    if (length >= MAXLEN)
        return;
    do
    {
        food_x = rng_range(&rng, 0, COLS - 1);
        food_y = rng_range(&rng, 0, ROWS - 1);
    } while (occupied[food_y * COLS + food_x]);
}

static void new_game(void)
{
    memset(occupied, 0, sizeof occupied);
    length = 3;
    head = length - 1;
    for (int i = 0; i < length; i++)
    {
        body_x[i] = COLS / 2 - (length - 1) + i;
        body_y[i] = ROWS / 2;
        occupied[body_y[i] * COLS + body_x[i]] = 1;
    }
    dir_x = 1; dir_y = 0;
    want_x = 1; want_y = 0;
    score = 0;
    crashed = 0;
    place_food();
}

// Would the head, moving by (dx, dy), hit a wall or the body? The tail cell is free unless the move eats.
static int blocked(int dx, int dy)
{
    int nx = body_x[head] + dx, ny = body_y[head] + dy;
    if (nx < 0 || nx >= COLS || ny < 0 || ny >= ROWS)
        return 1;
    if (!occupied[ny * COLS + nx])
        return 0;
    int t = tail_index();
    int eats = nx == food_x && ny == food_y;
    return !(nx == body_x[t] && ny == body_y[t] && !eats);
}

// One step. Returns 1 when it ate the food.
static int move_snake(void)
{
    if (!(want_x == -dir_x && want_y == -dir_y))    // it cannot turn back on itself
    {
        dir_x = want_x;
        dir_y = want_y;
    }
    if (blocked(dir_x, dir_y))
    {
        crashed = 1;
        return 0;
    }
    int nx = body_x[head] + dir_x, ny = body_y[head] + dir_y;
    int eats = nx == food_x && ny == food_y;
    if (!eats)
    {
        int t = tail_index();
        occupied[body_y[t] * COLS + body_x[t]] = 0; // the tail leaves
    }
    else
        length++;
    head = (head + 1) % MAXLEN;
    body_x[head] = nx;
    body_y[head] = ny;
    occupied[ny * COLS + nx] = 1;
    if (eats)
    {
        score += 10;
        place_food();
    }
    return eats;
}

static int speed_frames(void)               // frames per step: 8 at the start, 3 after 250 points
{
    int faster = score / 50;
    return 8 - (faster > 5 ? 5 : faster);
}

static void draw(void)
{
    gfx_clear(RGB(10, 14, 10));
    gfx_rect(0, 0, COLS * CELL, ROWS * CELL, RGB(40, 90, 40));
    gfx_circle_fill(food_x * CELL + CELL / 2, food_y * CELL + CELL / 2, CELL / 2 - 1, RGB(230, 60, 50));
    for (int i = 0; i < length; i++)
    {
        int k = (head - i + MAXLEN) % MAXLEN;
        unsigned int c = i == 0 ? RGB(190, 255, 120) : color_hsv(110 - i * 2, 200, 220);
        gfx_rect_fill(body_x[k] * CELL + 1, body_y[k] * CELL + 1, CELL - 2, CELL - 2, c);
    }
    font_printf(NULL, 4, 3, RGB(220, 220, 220), 1, "score %d", score);
    if (crashed)
    {
        gfx_rect_fill(60, 70, 200, 60, RGB(0, 0, 0));
        gfx_rect(60, 70, 200, 60, RGB(200, 200, 200));
        font_text(NULL, 96, 80, "GAME OVER", RGB(255, 90, 80), 2);
        font_printf(NULL, 100, 102, RGB(220, 220, 220), 1, "score %d  -  Enter", score);
    }
    gfx_present();
}

#ifdef DEMO_FRAMES
// A bot: head for the food along the longer axis first, and take any free direction when that is blocked.
static void bot_steer(void)
{
    int dx = food_x - body_x[head], dy = food_y - body_y[head];
    int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    int sx = dx < 0 ? -1 : 1, sy = dy < 0 ? -1 : 1;
    int try_x[4], try_y[4];
    if (ax >= ay)
    {
        try_x[0] = sx; try_y[0] = 0;
        try_x[1] = 0;  try_y[1] = sy;
    }
    else
    {
        try_x[0] = 0;  try_y[0] = sy;
        try_x[1] = sx; try_y[1] = 0;
    }
    try_x[2] = 0;  try_y[2] = -sy;
    try_x[3] = -sx; try_y[3] = 0;
    for (int i = 0; i < 4; i++)
    {
        if ((try_x[i] == 0 && try_y[i] == 0) || (try_x[i] == -dir_x && try_y[i] == -dir_y))
            continue;
        if (!blocked(try_x[i], try_y[i]))
        {
            want_x = try_x[i];
            want_y = try_y[i];
            return;
        }
    }
}
#endif

int main(void)
{
    if (gfx_init(COLS * CELL, ROWS * CELL) != 0)
    {
        puts("snake: no display");
        return 1;
    }
    rng_seed(&rng, 2026);
    new_game();

    struct game g;
#ifdef DEMO_FRAMES
    game_init(&g, 0);
    int eaten = 0, crashes = 0, best = 0, longest = 3;
    for (int f = 0; f < DEMO_FRAMES; f++)
    {
        game_frame_begin(&g);
        bot_steer();
        eaten += move_snake();
        if (score > best) best = score;
        if (length > longest) longest = length;
        if (crashed)
        {
            crashes++;
            new_game();
        }
        draw();
        game_frame_end(&g);
    }
    printf("frames %d, food eaten %d, best score %d, longest %d, crashes %d\n", DEMO_FRAMES, eaten, best, longest, crashes);
    return 0;
#else
    game_init(&g, 0);
    game_pace_ms(&g, 16);
    input_terminal_fallback(1);
    int frames = 0;
    while (g.running)
    {
        game_frame_begin(&g);
        if (key_pressed(KEY_ESCAPE))
            game_quit(&g);
        if (key_pressed(KEY_UP) || key_pressed(KEY_W) || pad_pressed(GP_BTN_DPAD_UP))         { want_x = 0;  want_y = -1; }
        if (key_pressed(KEY_DOWN) || key_pressed(KEY_S) || pad_pressed(GP_BTN_DPAD_DOWN))     { want_x = 0;  want_y = 1; }
        if (key_pressed(KEY_LEFT) || key_pressed(KEY_A) || pad_pressed(GP_BTN_DPAD_LEFT))     { want_x = -1; want_y = 0; }
        if (key_pressed(KEY_RIGHT) || key_pressed(KEY_D) || pad_pressed(GP_BTN_DPAD_RIGHT))   { want_x = 1;  want_y = 0; }
        if (crashed)
        {
            if (key_pressed(KEY_ENTER))
                new_game();
        }
        else if (++frames % speed_frames() == 0)
            move_snake();
        draw();
        game_frame_end(&g);
    }
    return 0;
#endif
}
