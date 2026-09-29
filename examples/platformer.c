// A platformer on the `retro` profile (ceres run --profile retro): a 320x240 screen of tile layers and sprites
// (ceres/tiles.h, ceres/sprite.h), a 16 MHz CPU.
//
//   Left / Right   run
//   Z, Space, Up   jump
//   Esc            quit
//
// The level is 128 tiles wide, built from a strip of tiles (examples/art, made with tools/img2tiles.js) on layer 0;
// the hills and clouds behind are layer 1, which scrolls at half the speed, so they seem far away. The sky is the
// background colour, which the line table turns a shade lighter every eight lines. The hero and the coins are
// sprites - 16x16 with four frames, 8x8 spinning - that the GPU draws over the layers: nothing is erased or drawn
// again as they move. Positions are in sixteenths of a pixel, so the game plays the same on every run.
//
// Built with -DDEMO_FRAMES=n a bot plays for n frames - it runs right and jumps at walls and gaps - and asks for a
// Present every 90 frames, so `ceres run --frames` keeps those screens (examples/expected/platformer.frames).
#include "stdio.h"
#include "ceres/tiles.h"
#include "ceres/sprite.h"
#include "ceres/text.h"
#include "ceres/input.h"
#include "ceres/keys.h"
#include "art/platformer_tiles.h"
#include "art/platformer_hero.h"
#include "art/platformer_coin.h"

#define SCREEN_W 320
#define SCREEN_H 240
#define LEVEL_W 128              // tiles
#define LEVEL_H 30
#define GROUND 26                // the first row of the ground
#define SUB 16                   // sixteenths of a pixel

enum { GRASS, EARTH, BRICK, BLOCK, CLOUD_L, CLOUD_R, HILL_L, HILL_R, HILL };   // the strip's tiles, in order

static unsigned short level[LEVEL_W * 32];         // layer 0's map: 128x32
static unsigned short hills[64 * 32];              // layer 1's: 64x32
static unsigned char solid[LEVEL_H][LEVEL_W];
static struct line_entry sky[SCREEN_H / 8];
static unsigned int sprite_palette[32];

#define COINS 24
static int coin_x[COINS], coin_y[COINS];           // pixels, in the level
static int coin_taken[COINS];

// The hero: position and speed in sixteenths of a pixel.
static int hero_x = 3 * 8 * SUB, hero_y = (GROUND - 2) * 8 * SUB;
static int speed_x, speed_y;
static int on_ground, facing_left, run_frames;
static int coins, falls, frames;

static void put(int col, int row, int tile)
{
    level[row * LEVEL_W + col] = platformer_tiles_map[tile];
    solid[row][col] = 1;
}

static void platform(int col, int row, int length, int tile)
{
    for (int i = 0; i < length; i++)
        put(col + i, row, tile);
}

static void coin(int n, int col, int row)
{
    coin_x[n] = col * 8;
    coin_y[n] = row * 8;
}

// The ground with its gaps, walls of earth to jump, platforms of bricks and blocks, and the coins over them.
static void build_level(void)
{
    static const int gaps[][2] = { { 22, 3 }, { 47, 4 }, { 71, 3 }, { 95, 4 } };
    for (int col = 0; col < LEVEL_W; col++)
    {
        int gap = 0;
        for (int g = 0; g < 4; g++)
            gap |= col >= gaps[g][0] && col < gaps[g][0] + gaps[g][1];
        if (gap)
            continue;
        put(col, GROUND, GRASS);
        for (int row = GROUND + 1; row < LEVEL_H; row++)
            put(col, row, EARTH);
    }
    static const int walls[][2] = { { 14, 2 }, { 34, 3 }, { 60, 2 }, { 84, 3 }, { 110, 4 } };
    for (int w = 0; w < 5; w++)
        for (int h = 1; h <= walls[w][1]; h++)
            put(walls[w][0], GROUND - h, h == walls[w][1] ? GRASS : EARTH);
    platform(18, 20, 5, BRICK);
    platform(20, 16, 1, BLOCK);
    platform(40, 19, 6, BRICK);
    platform(52, 21, 4, BLOCK);
    platform(64, 18, 7, BRICK);
    platform(78, 20, 3, BLOCK);
    platform(88, 17, 6, BRICK);
    platform(102, 21, 5, BRICK);
    int n = 0;
    for (int i = 0; i < 5; i++)
        coin(n++, 18 + i, 18);
    for (int i = 0; i < 4; i++)
        coin(n++, 22 + i, 21);                     // over the first gap
    for (int i = 0; i < 6; i++)
        coin(n++, 40 + i, 17);
    for (int i = 0; i < 5; i++)
        coin(n++, 64 + i, 16);
    for (int i = 0; i < 4; i++)
        coin(n++, 96 + i, 21);                     // over the last gap
}

// The layer behind: a range of hills along the bottom and two clouds, repeating every 64 tiles.
static void build_hills(void)
{
    for (int col = 0; col < 64; col += 8)
    {
        const int top = 22 + (col / 8) % 3;
        for (int row = top; row < 32; row++)
            for (int i = 0; i < 6; i++)
                hills[row * 64 + col + i] = platformer_tiles_map[row == top ? (i < 3 ? HILL_L : HILL_R) : HILL];
    }
    static const int clouds[][2] = { { 5, 4 }, { 21, 7 }, { 38, 3 }, { 52, 6 } };
    for (int c = 0; c < 4; c++)
    {
        hills[clouds[c][1] * 64 + clouds[c][0]] = platformer_tiles_map[CLOUD_L];
        hills[clouds[c][1] * 64 + clouds[c][0] + 1] = platformer_tiles_map[CLOUD_R];
    }
}

static int solid_at(int px, int py)                 // a pixel of the level
{
    if (px < 0 || px >= LEVEL_W * 8)
        return 1;
    if (py < 0 || py >= LEVEL_H * 8)
        return 0;
    return solid[py / 8][px / 8];
}

// The hero's box: 10 pixels wide in the middle of the sprite, 15 high.
static int blocked(int x, int y)
{
    const int left = x / SUB + 3, right = x / SUB + 12, top = y / SUB + 1, bottom = y / SUB + 15;
    return solid_at(left, top) || solid_at(right, top) || solid_at(left, bottom) || solid_at(right, bottom) ||
        solid_at(left, (top + bottom) / 2) || solid_at(right, (top + bottom) / 2);
}

static void step(int left, int right, int jump)
{
    const int target = right ? 24 : left ? -24 : 0;
    speed_x += speed_x < target ? 3 : speed_x > target ? -3 : 0;
    if (right)
        facing_left = 0;
    if (left)
        facing_left = 1;
    if (jump && on_ground)
        speed_y = -92;
    speed_y += 6;
    if (speed_y > 96)
        speed_y = 96;

    // Across, then up or down, each stopped a pixel at a time at what is solid.
    for (int moved = 0; moved != speed_x;)
    {
        const int d = speed_x - moved > SUB ? SUB : speed_x - moved < -SUB ? -SUB : speed_x - moved;
        if (blocked(hero_x + d, hero_y))
        {
            speed_x = 0;
            break;
        }
        hero_x += d;
        moved += d;
    }
    on_ground = 0;
    for (int moved = 0; moved != speed_y;)
    {
        const int d = speed_y - moved > SUB ? SUB : speed_y - moved < -SUB ? -SUB : speed_y - moved;
        if (blocked(hero_x, hero_y + d))
        {
            on_ground = speed_y > 0;
            speed_y = 0;
            break;
        }
        hero_y += d;
        moved += d;
    }
    if (hero_y / SUB > SCREEN_H)                      // down a gap: back a little before it
    {
        falls++;
        hero_x = (hero_x / SUB > 48 ? hero_x - 48 * SUB : 3 * 8 * SUB);
        hero_y = (GROUND - 6) * 8 * SUB;
        speed_x = speed_y = 0;
    }
    run_frames = speed_x != 0 && on_ground ? run_frames + 1 : 0;
}

#ifdef DEMO_FRAMES
// The bot of the demo: it runs right and jumps when a wall is ahead or the ground ends ahead.
static void bot(int* right, int* jump)
{
    const int x = hero_x / SUB, feet = hero_y / SUB + 15;
    const int wall = solid_at(x + 20, feet - 4) || solid_at(x + 20, feet - 12);
    const int gap = !solid_at(x + 14, feet + 4) && !solid_at(x + 22, feet + 4);
    *right = 1;
    *jump = wall || gap;
}
#endif

static void draw(int camera)
{
    tiles_scroll(0, camera, 0);
    tiles_scroll(1, camera / 2, 0);
    const int frame = !on_ground ? 2 : run_frames == 0 ? 0 : 1 + (run_frames / 6) % 3;
    sprite_set(0, hero_x / SUB - camera, hero_y / SUB, platformer_hero_graphic[frame],
        PLATFORMER_HERO_ATTRIBUTES | SPRITE_BANK(platformer_hero_bank[frame]) | SPRITE_PRIORITY(3) | (facing_left ? SPRITE_FLIP_X : 0));
    const int spin = (frames / 8) % PLATFORMER_COIN_FRAMES;
    const unsigned int first_coin = sizeof platformer_hero_pictures / 32;
    for (int i = 0; i < COINS; i++)
    {
        const int x = coin_x[i] - camera;
        if (coin_taken[i] || x < -8 || x >= SCREEN_W)
            sprite_hide(1 + i);
        else
            sprite_set(1 + i, x, coin_y[i], first_coin + platformer_coin_graphic[spin],
                PLATFORMER_COIN_ATTRIBUTES | SPRITE_BANK(1 + platformer_coin_bank[spin]) | SPRITE_PRIORITY(3));
    }
    sprites_commit();
}

static void take_coins(void)
{
    const int x = hero_x / SUB, y = hero_y / SUB;
    for (int i = 0; i < COINS; i++)
        if (!coin_taken[i] && coin_x[i] + 8 > x + 3 && coin_x[i] < x + 13 && coin_y[i] + 8 > y && coin_y[i] < y + 16)
        {
            coin_taken[i] = 1;
            coins++;
        }
}

int main(void)
{
    if (tiles_init(SCREEN_W, SCREEN_H) != 0)
    {
        printf("platformer: this needs level V2 at 320x240 (ceres run --profile retro, or any larger one)\n");
        return 1;
    }
    mmio_w32(TEXT_ENABLE, 0u);

    build_level();
    build_hills();
    void* palette = video_vram_alloc(sizeof platformer_tiles_palette);
    void* tileset = video_vram_alloc(sizeof platformer_tiles_tiles);
    void* level_map = video_vram_alloc(sizeof level);
    void* hills_map = video_vram_alloc(sizeof hills);
    void* sprite_colours = video_vram_alloc(sizeof sprite_palette);
    void* pictures = video_vram_alloc(sizeof platformer_hero_pictures + sizeof platformer_coin_pictures);
    void* table = video_vram_alloc(sizeof sky);
    if (table == 0)
        return 1;
    tiles_load(palette, platformer_tiles_palette, sizeof platformer_tiles_palette);
    tiles_load(tileset, platformer_tiles_tiles, sizeof platformer_tiles_tiles);
    tiles_load(level_map, level, sizeof level);
    tiles_load(hills_map, hills, sizeof hills);
    // The hero's colours in bank 0, the coin's after them; the coin's pictures after the hero's.
    for (int i = 0; i < 16; i++)
    {
        sprite_palette[i] = platformer_hero_palette[i];
        sprite_palette[16 + i] = platformer_coin_palette[i];
    }
    tiles_load(sprite_colours, sprite_palette, sizeof sprite_palette);
    tiles_load(pictures, platformer_hero_pictures, sizeof platformer_hero_pictures);
    tiles_load((char*)pictures + sizeof platformer_hero_pictures, platformer_coin_pictures, sizeof platformer_coin_pictures);

    tiles_set_palette(palette);
    tiles_layer(0, LAYER_MAP(128, 32) | LAYER_PRIORITY(2), level_map, tileset);
    tiles_layer(1, LAYER_MAP(64, 32) | LAYER_PRIORITY(1), hills_map, tileset);
    sprites_init(sprite_colours, pictures);
    for (int i = 0; i < SCREEN_H / 8; i++)
    {
        const struct line_entry shade = LINE_ENTRY(i * 8, GPU_BACKGROUND, (unsigned int)(0x40 + i * 3) << 16 | (unsigned int)(0x70 + i * 4) << 8 | (unsigned int)(0xD0 + i));
        sky[i] = shade;
    }
    tiles_load(table, sky, sizeof sky);
    tiles_line_table(table, SCREEN_H / 8);

    input_init();
    int camera = 0;
    for (;;)
    {
        int left = 0, right = 0, jump = 0;
#ifdef DEMO_FRAMES
        if (frames >= DEMO_FRAMES)
            break;
        bot(&right, &jump);
#else
        input_update();
        if (key_down(KEY_ESCAPE))
            break;
        left = key_down(KEY_LEFT);
        right = key_down(KEY_RIGHT);
        jump = key_down(KEY_Z) || key_down(KEY_SPACE) || key_down(KEY_UP);
#endif
        step(left, right, jump);
        take_coins();
        camera = hero_x / SUB - 120;
        camera = camera < 0 ? 0 : camera > LEVEL_W * 8 - SCREEN_W ? LEVEL_W * 8 - SCREEN_W : camera;
        video_wait_vblank();
        draw(camera);
        frames++;
#ifdef DEMO_FRAMES
        if (frames % 90 == 0)
            video_present();
#endif
        if (hero_x / SUB >= (LEVEL_W - 4) * 8)
            break;                                     // the end of the level
    }

    mmio_w32(TEXT_ENABLE, 1u);
    printf("coins %d of %d, falls %d, frames %d, got to %d of %d\n", coins, COINS, falls, frames, hero_x / SUB, LEVEL_W * 8);
    return 0;
}
