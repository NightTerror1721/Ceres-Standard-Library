// A maze on the smallest machine, `micro` (ceres run --profile micro): a 2 MHz CPU, 64 KiB of RAM, 32 KiB of video
// memory and a 256x192 screen of tiles and sprites (ceres/tiles.h, ceres/sprite.h).
//
//   Arrows        walk
//   Esc           quit
//
// A new maze is dug for every game (a random walk that never crosses its own path, so there is exactly one way from
// any place to any other), drawn with three tiles on a tile layer - wall, star, and the dots of the way already
// walked - and the red one is a sprite. Through the floor, which is clear, shows the background colour, which the
// line table changes every four lines for a gradient.
//
// On `micro` the CPU may write the video memory only in the vertical blank (or with the screen off); a store at any
// other moment is lost and the GPU reports it. So the loads go through the copy engine, which may, and the dots are
// written by the CPU right after video_wait_vblank(), inside the blank. Whether any store was lost is printed at the
// end: none may be.
//
// Built with -DDEMO_FRAMES=n it walks the way out by itself (found with a breadth-first search) for n frames, through
// as many mazes as that takes, and asks for a Present every 128 frames, so `ceres run --frames` keeps those screens:
// examples/expected/maze.frames holds their hashes.
#include "stdio.h"
#include "ceres/tiles.h"
#include "ceres/sprite.h"
#include "ceres/text.h"
#include "ceres/input.h"
#include "ceres/keys.h"
#include "ceres/rand.h"
#include "art/maze_tiles.h"
#include "art/maze_hero.h"

#define COLS 31                  // the maze, in tiles: walls on the even columns and rows, ways on the odd ones
#define ROWS 23
#define MAP_SIDE 32              // the layer's map is 32x32 entries
#define LEFT 4                   // the maze's top-left corner on the screen, in pixels
#define TOP 4

static unsigned char wall[ROWS][COLS];
static unsigned short map[MAP_SIDE * MAP_SIDE];
static short came_from[ROWS * COLS];
static short way[ROWS * COLS];           // the way out, from the start
static int way_length;
static struct line_entry gradient[192 / 4];
static unsigned short* vram_map;         // the map in video memory
static struct rng rng;

// A random walk through the cells (the odd places), knocking down the wall between a cell and a new one; when it is
// stuck it goes back along its path to the last cell with somewhere new to go.
static void dig(void)
{
    static short stack[(ROWS / 2) * (COLS / 2)];
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            wall[r][c] = 1;
    int depth = 0;
    stack[depth++] = (short)(1 * COLS + 1);
    wall[1][1] = 0;
    while (depth > 0)
    {
        const int here = stack[depth - 1];
        const int r = here / COLS, c = here % COLS;
        int options[4], count = 0;
        static const int dr[4] = { -2, 2, 0, 0 }, dc[4] = { 0, 0, -2, 2 };
        for (int d = 0; d < 4; d++)
        {
            const int nr = r + dr[d], nc = c + dc[d];
            if (nr > 0 && nr < ROWS && nc > 0 && nc < COLS && wall[nr][nc])
                options[count++] = d;
        }
        if (count == 0)
        {
            depth--;
            continue;
        }
        const int d = options[rng_range(&rng, 0, count - 1)];
        wall[r + dr[d] / 2][c + dc[d] / 2] = 0;
        wall[r + dr[d]][c + dc[d]] = 0;
        stack[depth++] = (short)((r + dr[d]) * COLS + c + dc[d]);
    }
}

// The way from the start (1, 1) to the star (ROWS - 2, COLS - 2): breadth first, then back along came_from.
static void find_way(void)
{
    static short queue[ROWS * COLS];
    for (int i = 0; i < ROWS * COLS; i++)
        came_from[i] = -1;
    const int start = 1 * COLS + 1, goal = (ROWS - 2) * COLS + COLS - 2;
    int head = 0, tail = 0;
    queue[tail++] = (short)start;
    came_from[start] = (short)start;
    while (head < tail)
    {
        const int here = queue[head++];
        if (here == goal)
            break;
        const int next[4] = { here - COLS, here + COLS, here - 1, here + 1 };
        for (int d = 0; d < 4; d++)
            if (!wall[next[d] / COLS][next[d] % COLS] && came_from[next[d]] < 0)
            {
                came_from[next[d]] = (short)here;
                queue[tail++] = (short)next[d];
            }
    }
    way_length = 0;
    for (int at = goal; at != start; at = came_from[at])
        way[way_length++] = (short)at;
    for (int i = 0; i < way_length / 2; i++)
    {
        const short t = way[i];
        way[i] = way[way_length - 1 - i];
        way[way_length - 1 - i] = t;
    }
}

// The maze into the map (the entries img2tiles made for each tile of the strip), and the map into video memory.
static void draw_maze(void)
{
    for (int i = 0; i < MAP_SIDE * MAP_SIDE; i++)
        map[i] = 0;
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            if (wall[r][c])
                map[r * MAP_SIDE + c] = maze_tiles_map[0];
    map[(ROWS - 2) * MAP_SIDE + COLS - 2] = maze_tiles_map[1];
    tiles_load(vram_map, map, sizeof map);
}

static int frames;
static int steps;
static int escapes;

// printf would bring its formatting of doubles along, some 35 KB of a machine that has 64: the few numbers this
// prints are written out by hand, and puts() does the rest.
static char* put_text(char* at, const char* text)
{
    while (*text)
        *at++ = *text++;
    return at;
}

static char* put_number(char* at, int n)
{
    char digits[12];
    int count = 0;
    do
    {
        digits[count++] = (char)('0' + n % 10);
        n /= 10;
    } while (n > 0);
    while (count > 0)
        *at++ = digits[--count];
    return at;
}

// Walks the hero from cell (r, c) to the next one, a pixel a frame, then drops a dot where it was.
static void walk(int r, int c, int nr, int nc)
{
    for (int i = 1; i <= 8; i++)
    {
        video_wait_vblank();
        frames++;
        if (i == 8)
            vram_map[r * MAP_SIDE + c] = maze_tiles_map[2];   // the CPU's own store, in the blank
        const int x = LEFT + c * 8 + (nc - c) * i, y = TOP + r * 8 + (nr - r) * i;
        const int frame = (i / 4) & 1;
        sprite_set(0, x, y, maze_hero_graphic[frame], MAZE_HERO_ATTRIBUTES | SPRITE_BANK(maze_hero_bank[frame]) | SPRITE_PRIORITY(1));
        sprites_commit();
#ifdef DEMO_FRAMES
        if (frames % 128 == 0)
            video_present();
#endif
    }
    steps++;
}

int main(void)
{
    if (tiles_init(256, 192) != 0)
    {
        puts("maze: this needs level V2 at 256x192 (ceres run --profile micro, or any larger one)");
        return 1;
    }
    mmio_w32(TEXT_ENABLE, 0u);

    void* palette = video_vram_alloc(sizeof maze_tiles_palette);
    void* tiles = video_vram_alloc(sizeof maze_tiles_tiles);
    void* sprite_palette = video_vram_alloc(sizeof maze_hero_palette);
    void* pictures = video_vram_alloc(sizeof maze_hero_pictures);
    void* table = video_vram_alloc(sizeof gradient);
    vram_map = (unsigned short*)video_vram_alloc(sizeof map);
    if (vram_map == 0)
        return 1;
    tiles_load(palette, maze_tiles_palette, sizeof maze_tiles_palette);
    tiles_load(tiles, maze_tiles_tiles, sizeof maze_tiles_tiles);
    tiles_load(sprite_palette, maze_hero_palette, sizeof maze_hero_palette);
    tiles_load(pictures, maze_hero_pictures, sizeof maze_hero_pictures);
    tiles_set_palette(palette);
    tiles_layer(0, LAYER_MAP(32, 32) | LAYER_PRIORITY(0), vram_map, tiles);
    tiles_scroll(0, -LEFT, -TOP);
    sprites_init(sprite_palette, pictures);

    // Dark blue at the top to purple at the bottom, a step every four lines.
    for (int i = 0; i < 192 / 4; i++)
    {
        gradient[i].head = (unsigned int)(i * 4) | ((GPU_BACKGROUND - GPU_BASE) << 16);
        gradient[i].value = (unsigned int)(0x10 + i) << 16 | (unsigned int)(0x08 + i / 2) << 8 | (unsigned int)(0x40 + i);
    }
    tiles_load(table, gradient, sizeof gradient);
    tiles_line_table(table, 192 / 4);

    input_init();
    rng_seed(&rng, 1987);
    int quit = 0;
    while (!quit)
    {
        dig();
        find_way();
        draw_maze();
        int r = 1, c = 1;
        walk(r, c, r, c);
#ifdef DEMO_FRAMES
        for (int i = 0; i < way_length && frames < DEMO_FRAMES; i++)
        {
            const int nr = way[i] / COLS, nc = way[i] % COLS;
            walk(r, c, nr, nc);
            r = nr;
            c = nc;
        }
        quit = frames >= DEMO_FRAMES;
#else
        while (!(r == ROWS - 2 && c == COLS - 2) && !quit)
        {
            input_update();
            int dr = 0, dc = 0;
            if (key_down(KEY_UP)) dr = -1;
            else if (key_down(KEY_DOWN)) dr = 1;
            else if (key_down(KEY_LEFT)) dc = -1;
            else if (key_down(KEY_RIGHT)) dc = 1;
            quit = key_down(KEY_ESCAPE);
            if ((dr || dc) && !wall[r + dr][c + dc])
            {
                walk(r, c, r + dr, c + dc);
                r += dr;
                c += dc;
            }
            else
                video_wait_vblank();
        }
#endif
        if (r == ROWS - 2 && c == COLS - 2)
            escapes++;
    }

    mmio_w32(TEXT_ENABLE, 1u);
    char line[96];
    char* at = put_text(line, "mazes left ");
    at = put_number(at, escapes);
    at = put_text(at, ", steps ");
    at = put_number(at, steps);
    at = put_text(at, ", frames ");
    at = put_number(at, frames);
    at = put_text(at, ", stores to video memory lost: ");
    at = put_text(at, mmio_r32(GPU_FAULT_CODE) == 2u ? "some" : "none");
    *at = 0;
    puts(line);
    return 0;
}
