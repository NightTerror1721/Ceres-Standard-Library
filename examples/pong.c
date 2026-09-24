// Pong on the pixel display, first to 5 wins.
//
//   W / S, Up / Down, or the gamepad's left stick   move your paddle (the left one)
//   Enter                                           play again after a match
//   Esc                                             quit (closing the window also ends it)
//
// The ball's position and speed are 16.16 fixed-point numbers (ceres/fixed.h): the same game plays out the
// same way at every optimization level. Where the ball meets the paddle decides its return angle, and every
// return is a little faster. The right paddle is a simple computer player. The frame loop is paced by
// sleeping (game_pace_ms).
//
// Built with -DDEMO_FRAMES=4000 a bot plays your side as well - no window, no pacing - and the program
// prints the record of the matches: that is what examples/expected/pong.expected holds.
#include "stdio.h"
#include "ceres/gfx.h"
#include "ceres/font.h"
#include "ceres/game.h"
#include "ceres/input.h"
#include "ceres/fixed.h"
#include "ceres/rand.h"
#include "ceres/sprite.h"

#define W 320
#define H 200
#define PADDLE_W 6
#define PADDLE_H 40
#define BALL 6
#define LEFT_X 10
#define RIGHT_X (W - 10 - PADDLE_W)
#define WIN_SCORE 5
#define SPEED_UP 69468                      // 1.06 in 16.16: each return is 6% faster
#define MAX_VX 458752                       // 7.0 pixels a frame

static int left_y, right_y;                 // top edges of the paddles
static fixed_t ball_x, ball_y, ball_vx, ball_vy;
static int score_l, score_r;
static int rally;                           // returns since the serve
static int aim_l, aim_r;                    // how far off the ball each computer player aims, changed at every return
static int over;                            // a match has been won
static struct rng rng;

static int ball_center_y(void)
{
    return FIX_TO_INT(ball_y) + BALL / 2;
}

static void new_aim(void)
{
    aim_l = rng_range(&rng, -30, 30);       // beyond +-23 the ball misses the paddle: it is beaten now and then
    aim_r = rng_range(&rng, -30, 30);
}

static void serve(int dir)                  // dir: +1 toward the right paddle, -1 toward the left
{
    ball_x = FIX_INT(W / 2 - BALL / 2);
    ball_y = FIX_INT(H / 2 - BALL / 2);
    ball_vx = FIX_INT(3) * dir;
    int v = rng_range(&rng, 1, 2);
    ball_vy = FIX_INT(rng_chance(&rng, 50) ? v : -v);
    rally = 0;
    new_aim();
}

static void new_match(void)
{
    left_y = right_y = H / 2 - PADDLE_H / 2;
    score_l = score_r = 0;
    over = 0;
    serve(1);
}

static int clamp_paddle(int y)
{
    return y < 0 ? 0 : (y > H - PADDLE_H ? H - PADDLE_H : y);
}

// The computer player: follows the ball when it is coming, drifts back to the middle when it is not.
static int follow(int paddle_y, int coming, int speed, int aim)
{
    int target = coming ? ball_center_y() + aim : H / 2;
    int center = paddle_y + PADDLE_H / 2;
    if (target - center > 4)
        paddle_y += speed;
    else if (target - center < -4)
        paddle_y -= speed;
    return clamp_paddle(paddle_y);
}

static void bounce(int paddle_y, int edge_x)
{
    ball_vx = fx_mul(-ball_vx, SPEED_UP);
    if (ball_vx > MAX_VX) ball_vx = MAX_VX;
    if (ball_vx < -MAX_VX) ball_vx = -MAX_VX;
    int off_center = ball_center_y() - (paddle_y + PADDLE_H / 2);      // -23 .. 23
    ball_vy = fx_div(FIX_INT(off_center), FIX_INT(9));
    ball_x = FIX_INT(edge_x);
    rally++;
    new_aim();
}

// One step of the ball. Returns +1 when the left side scored, -1 when the right did, 0 otherwise.
static int step_ball(int* hits)
{
    ball_x += ball_vx;
    ball_y += ball_vy;
    int y = FIX_TO_INT(ball_y);
    if (y < 0)
    {
        ball_y = -ball_y;
        ball_vy = -ball_vy;
    }
    else if (y + BALL > H)
    {
        ball_y = FIX_INT(2 * (H - BALL)) - ball_y;
        ball_vy = -ball_vy;
    }

    struct rect b = { FIX_TO_INT(ball_x), FIX_TO_INT(ball_y), BALL, BALL };
    struct rect pl = { LEFT_X, left_y, PADDLE_W, PADDLE_H };
    struct rect pr = { RIGHT_X, right_y, PADDLE_W, PADDLE_H };
    if (ball_vx < 0 && rect_overlap(&b, &pl))
    {
        bounce(left_y, LEFT_X + PADDLE_W);
        (*hits)++;
    }
    else if (ball_vx > 0 && rect_overlap(&b, &pr))
    {
        bounce(right_y, RIGHT_X - BALL);
        (*hits)++;
    }

    if (FIX_TO_INT(ball_x) + BALL < 0)
        return -1;
    if (FIX_TO_INT(ball_x) > W)
        return 1;
    return 0;
}

static void draw(void)
{
    gfx_clear(RGB(0, 0, 0));
    for (int y = 4; y < H; y += 12)
        gfx_rect_fill(W / 2 - 1, y, 2, 6, RGB(70, 70, 70));
    gfx_rect_fill(LEFT_X, left_y, PADDLE_W, PADDLE_H, RGB(230, 230, 230));
    gfx_rect_fill(RIGHT_X, right_y, PADDLE_W, PADDLE_H, RGB(230, 230, 230));
    gfx_rect_fill(FIX_TO_INT(ball_x), FIX_TO_INT(ball_y), BALL, BALL, RGB(255, 220, 90));
    font_printf(NULL, W / 2 - 48, 8, RGB(160, 160, 160), 4, "%d", score_l);
    font_printf(NULL, W / 2 + 24, 8, RGB(160, 160, 160), 4, "%d", score_r);
    if (over)
    {
        gfx_rect_fill(70, 80, 180, 40, RGB(0, 0, 0));
        gfx_rect(70, 80, 180, 40, RGB(200, 200, 200));
        font_text(NULL, 96, 88, score_l > score_r ? "YOU WIN" : "YOU LOSE", RGB(255, 220, 90), 2);
        font_text(NULL, 106, 108, "Enter: again", RGB(200, 200, 200), 1);
    }
    gfx_present();
}

int main(void)
{
    if (gfx_init(W, H) != 0)
    {
        puts("pong: no display");
        return 1;
    }
    rng_seed(&rng, 2026);
    new_match();

    struct game g;
#ifdef DEMO_FRAMES
    game_init(&g, 0);
    int matches = 0, left_wins = 0, right_wins = 0, points = 0, hits = 0, longest = 0;
    for (int f = 0; f < DEMO_FRAMES; f++)
    {
        game_frame_begin(&g);
        left_y = follow(left_y, ball_vx < 0, 3, aim_l);            // a bot on each side; the left one is a little quicker
        right_y = follow(right_y, ball_vx > 0, 2, aim_r);
        int scored = step_ball(&hits);
        if (rally > longest) longest = rally;
        if (scored != 0)
        {
            points++;
            if (scored > 0) score_l++; else score_r++;
            if (score_l == WIN_SCORE || score_r == WIN_SCORE)
            {
                matches++;
                if (score_l > score_r) left_wins++; else right_wins++;
                new_match();
            }
            else
                serve(scored > 0 ? -1 : 1);
        }
        draw();
        game_frame_end(&g);
    }
    printf("frames %d, matches %d (left %d, right %d), points %d, returns %d, longest rally %d\n",
           DEMO_FRAMES, matches, left_wins, right_wins, points, hits, longest);
    return 0;
#else
    game_init(&g, 0);
    game_pace_ms(&g, 16);
    input_terminal_fallback(1);
    int hits = 0;
    while (g.running)
    {
        game_frame_begin(&g);
        if (key_pressed(KEY_ESCAPE))
            game_quit(&g);
        if (over)
        {
            if (key_pressed(KEY_ENTER))
                new_match();
        }
        else
        {
            int move = 0;
            if (key_down(KEY_W) || key_down(KEY_UP)) move -= 3;
            if (key_down(KEY_S) || key_down(KEY_DOWN)) move += 3;
            move += (int)(pad_axis(PAD_AXIS_LEFT_Y) * 3.0f);
            left_y = clamp_paddle(left_y + move);
            right_y = follow(right_y, ball_vx > 0, 2, aim_r);
            int scored = step_ball(&hits);
            if (scored != 0)
            {
                if (scored > 0) score_l++; else score_r++;
                if (score_l == WIN_SCORE || score_r == WIN_SCORE)
                    over = 1;
                else
                    serve(scored > 0 ? -1 : 1);
            }
        }
        draw();
        game_frame_end(&g);
    }
    return 0;
#endif
}
