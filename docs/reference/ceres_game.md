# `<ceres/game.h>`

A fixed-step game loop: input -> logic -> drawing -> present -> wait for the next frame.

```c
struct game g;
game_init(&g, 200000);                  // a frame is 200000 instructions
while (g.running) {
    game_frame_begin(&g);               // input_update() and the start mark
    ...update, draw, gfx_present()...
    game_frame_end(&g);                 // wait out the rest of the frame
}
// or: game_run(&g, update, draw, ctx);
```

TIME. The machine's clock is the instruction counter, so by default a frame is a BUDGET OF INSTRUCTIONS and waiting is a spin on timer_ticks(): the same program does the same thing on every run, which is what a test wants. The same game has more room per frame at -O2 than at -O0, and how fast a frame is in seconds depends on the host.

For a game that must run at a steady speed on a real clock there are two ways, and in both a frame lasts `ms` milliseconds from its start - its own work counts, and a frame that runs late is not made up for:

```c
game_pace_ms(&g, 16)     sleeps: halts with the timer's alarm at the frame's end (timer_wait_until_ns64), so
                         the host is not kept busy. Needs no handler and no module;
game_pace_real(&g, 16)   spins on the nanosecond clock instead: the host is kept busy, and the frame ends
                         to the clock's step rather than the host's sleep's. Both are about 60 frames a second.
```

```c
struct game
{
    int running;                       // cleared by game_quit
    unsigned int frame;                // frames completed
    unsigned int ticks_per_frame;      // the instruction budget of a frame
    unsigned int frame_start;          // timer_ticks() when the frame began
    unsigned int work_last;            // instructions the last frame spent before waiting
    unsigned int wait_ms;              // 0: pace by instructions; otherwise the frame period in milliseconds
    uint64_t frame_start_ns;           // timer_nanos64() when the frame began
    void (*wait)(struct game* g);      // what game_frame_end runs to pass the rest of the frame; 0 spins on the budget
};

typedef void (*game_fn)(struct game* g, void* ctx);

void game_init(struct game* g, unsigned int ticks_per_frame);   // also input_init()
void game_quit(struct game* g);
void game_frame_begin(struct game* g);      // input_update(), and the mark work is measured from
void game_frame_end(struct game* g);        // counts the frame and waits out the rest of it
int  game_over_budget(const struct game* g);   // 1 when the last frame spent more than its budget (meaningful when pacing by instructions)
void game_run(struct game* g, game_fn update, game_fn draw, void* ctx);   // until game_quit

void game_pace_real(struct game* g, unsigned int ms);   // wall-clock pacing by spinning on the nanosecond clock
void game_pace_ms(struct game* g, unsigned int ms);     // wall-clock pacing by sleeping; ms is at least 1
```
