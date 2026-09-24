// The frame loop. See ceres/game.h.
#include "ceres/game.h"
#include "ceres/input.h"
#include "ceres/timer.h"

void game_init(struct game* g, unsigned int ticks_per_frame)
{
    g->running = 1;
    g->frame = 0;
    g->ticks_per_frame = ticks_per_frame;
    g->frame_start = timer_ticks();
    g->work_last = 0;
    g->wait_ms = 0;
    g->frame_start_ns = timer_nanos64();
    g->wait = 0;
    input_init();
}

void game_quit(struct game* g)
{
    g->running = 0;
}

void game_frame_begin(struct game* g)
{
    input_update();
    g->frame_start = timer_ticks();
    g->frame_start_ns = timer_nanos64();
}

void game_frame_end(struct game* g)
{
    g->work_last = timer_elapsed(g->frame_start);
    g->frame++;
    if (g->wait != 0)
    {
        g->wait(g);
        return;
    }
    while (timer_elapsed(g->frame_start) < g->ticks_per_frame)
    {
    }
}

// The frame ends `wait_ms` after it began. A frame already past that does not wait, and the next one starts
// from now: a slow frame is not repaid with a burst of fast ones.
static uint64_t frame_deadline(const struct game* g)
{
    return g->frame_start_ns + (uint64_t)g->wait_ms * 1000000u;
}

static void spin_nanos(struct game* g)
{
    uint64_t deadline = frame_deadline(g);
    while (timer_nanos64() < deadline)
    {
    }
}

// A key or the mouse ends a halt early; timer_wait_until_ns64 halts again for the time that is left.
static void sleep_nanos(struct game* g)
{
    timer_wait_until_ns64(frame_deadline(g));
}

void game_pace_real(struct game* g, unsigned int ms)
{
    g->wait_ms = ms;
    g->wait = spin_nanos;
}

void game_pace_ms(struct game* g, unsigned int ms)
{
    g->wait_ms = ms < 1u ? 1u : ms;
    g->wait = sleep_nanos;
}

int game_over_budget(const struct game* g)
{
    return g->work_last > g->ticks_per_frame;
}

void game_run(struct game* g, game_fn update, game_fn draw, void* ctx)
{
    while (g->running)
    {
        game_frame_begin(g);
        if (update != 0)
            update(g, ctx);
        if (draw != 0 && g->running)
            draw(g, ctx);
        game_frame_end(g);
    }
}
