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
