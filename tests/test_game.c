// The frame loop paced by a tick budget: a frame takes at least its budget, the counters move as
// documented, and game_run calls update and draw once per frame until game_quit. (The timer counts CPU
// cycles, not source lines: a load or a store costs 2, so an empty frame is about 600 of them at -O0 and 150
// at -O2. So the checks are about the WAIT, measured around game_frame_end, and allow the work a frame does
// to vary with the optimization level.)
#include "ceres/test.h"
#include "ceres/game.h"
#include "ceres/timer.h"

struct tally { int updates; int draws; int last_frame_seen; int quit_at; };

static void update(struct game* g, void* ctx)
{
    struct tally* t = (struct tally*)ctx;
    t->updates++;
    t->last_frame_seen = (int)g->frame;
    if (t->updates == t->quit_at)
        game_quit(g);
}

static void draw(struct game* g, void* ctx)
{
    ((struct tally*)ctx)->draws++;
}

int main(void)
{
    struct game g;

    TEST_SECTION("init");
    game_init(&g, 30000);
    CHECK_EQ(g.running, 1);
    CHECK_EQ((int)g.frame, 0);
    CHECK_EQ((int)g.ticks_per_frame, 30000);
    CHECK_EQ((int)g.work_last, 0);
    CHECK_EQ((int)g.wait_ms, 0);
    CHECK(g.wait == 0);

    TEST_SECTION("a frame lasts at least its budget");
    int at_least = 1, not_much_more = 1;
    for (int i = 0; i < 5; i++)
    {
        game_frame_begin(&g);
        unsigned int before = timer_ticks();
        game_frame_end(&g);
        unsigned int after_end = timer_elapsed(g.frame_start);      // the whole frame, from its start mark
        if (after_end < 30000u) at_least = 0;
        if (after_end > 30000u + 400u) not_much_more = 0;           // the wait stops the moment the budget is spent
        (void)before;
    }
    CHECK_EQ((int)g.frame, 5);
    CHECK(at_least);
    CHECK(not_much_more);
    CHECK(g.work_last < 1200);                          // an empty frame does almost no work
    CHECK_EQ(game_over_budget(&g), 0);

    TEST_SECTION("work is measured, and a heavy frame is over budget");
    game_frame_begin(&g);
    volatile int sink = 0;
    for (int i = 0; i < 3000; i++)
        sink += i;
    game_frame_end(&g);
    CHECK(g.work_last > 30000);
    CHECK_EQ(game_over_budget(&g), 1);
    CHECK_EQ((int)g.frame, 6);

    TEST_SECTION("game_run");
    struct tally t;
    t.updates = 0; t.draws = 0; t.last_frame_seen = -1; t.quit_at = 7;
    game_init(&g, 500);
    game_run(&g, update, draw, &t);
    CHECK_EQ(t.updates, 7);
    CHECK_EQ(t.draws, 6);                               // the frame that quits does not draw
    CHECK_EQ(t.last_frame_seen, 6);                     // frames counted from 0
    CHECK_EQ((int)g.frame, 7);                          // the last frame was still ended
    CHECK_EQ(g.running, 0);

    TEST_SECTION("game_run without callbacks");
    t.updates = 0; t.draws = 0; t.quit_at = 3;
    game_init(&g, 100);
    game_run(&g, update, 0, &t);                        // no draw callback
    CHECK_EQ(t.updates, 3);
    CHECK_EQ(t.draws, 0);
    game_init(&g, 100);
    game_quit(&g);
    game_run(&g, 0, 0, 0);                              // not running: returns at once
    CHECK_EQ((int)g.frame, 0);

    TEST_SECTION("a zero budget does not wait");
    game_init(&g, 0);
    int quick = 1;
    for (int i = 0; i < 20; i++)
    {
        game_frame_begin(&g);
        unsigned int t0 = timer_ticks();
        game_frame_end(&g);
        if (timer_elapsed(t0) > 1000u) quick = 0;
    }
    CHECK_EQ((int)g.frame, 20);
    CHECK(quick);

    return test_summary();
}
