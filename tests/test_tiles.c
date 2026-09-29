// The GPU's level V2 from C (ceres/tiles.h, ceres/sprite.h): the program's video memory, the layers' registers, the
// affine layer's matrix, the line table, and the sprites - their copy in RAM, the commit into the OAM, and the limit
// of sprites a line the GPU reports after a frame. It runs on the `retro` profile (tests/expected/test_tiles.run): 32
// sprites a line.
#include "ceres/test.h"
#include "ceres/sprite.h"
#include "ceres/fb.h"

static const unsigned short map_data[4] = { TILE(1, 2), TILE(3, 0) | TILE_FLIP_X, TILE(0x3FF, 7) | TILE_FRONT, 0 };

int main(void)
{
    TEST_SECTION("video memory");
    const unsigned int free_at_start = video_vram_free();
    CHECK(free_at_start > 0);
    void* a = video_vram_alloc(100);
    void* b = video_vram_alloc(1);
    CHECK(a != 0 && b != 0);
    CHECK((unsigned int)a >= VRAM_BASE);
    CHECK_EQ((unsigned int)a & 255u, 0u);
    CHECK_EQ((unsigned int)b - (unsigned int)a, 256u);       // rounded up to 256
    CHECK_EQ(video_vram_free(), free_at_start - 512u);
    CHECK(video_vram_alloc(0) == 0);
    CHECK(video_vram_alloc(free_at_start) == 0);             // more than is left
    CHECK(video_vram_alloc(0xFFFFFFF0u) == 0);
    const unsigned int resets = video_vram_resets();
    video_vram_reset();
    CHECK_EQ(video_vram_resets(), resets + 1);
    CHECK_EQ(video_vram_free(), free_at_start);
    CHECK(video_vram_alloc(100) == a);

    TEST_SECTION("layers");
    CHECK_EQ(tiles_init(0, 10), -1);
    CHECK_EQ(tiles_init(256, 192), 0);
    CHECK_EQ(video_level(), VIDEO_RETRO);
    CHECK_EQ(video_width(), 256);
    void* map = video_vram_alloc(sizeof map_data);
    CHECK_EQ(tiles_load(map, map_data, sizeof map_data), 0);
    CHECK_EQ(((const unsigned short*)map)[0], 0x0801);       // tile 1, palette 2
    CHECK_EQ(((const unsigned short*)map)[1], 0x2003);
    CHECK_EQ(((const unsigned short*)map)[2], 0x9FFF);
    tiles_layer(2, LAYER_MAP(64, 128) | LAYER_TILE16 | LAYER_PRIORITY(3) | LAYER_BANK(5), map, (void*)0xA0001000);
    CHECK_EQ(mmio_r32(TILE_LAYER_CONTROL(2)), 0x5273u);    // on, 16x16, priority 3, 64 by 128, bank 5
    CHECK_EQ(mmio_r32(TILE_LAYER_MAP(2)), (unsigned int)map);
    CHECK_EQ(mmio_r32(TILE_LAYER_TILES(2)), 0xA0001000u);
    tiles_scroll(2, -3, 70000);
    CHECK_EQ(mmio_r32(TILE_LAYER_SCROLL_X(2)), 0xFFFFFFFDu);
    CHECK_EQ(mmio_r32(TILE_LAYER_SCROLL_Y(2)), 70000u);
    tiles_line_scroll(2, (void*)0xA0002000);
    CHECK_EQ(mmio_r32(TILE_LAYER_CONTROL(2)) & LAYER_LINE_SCROLL, LAYER_LINE_SCROLL);
    CHECK_EQ(mmio_r32(TILE_LAYER_LINE_SCROLL(2)), 0xA0002000u);
    tiles_line_scroll(2, 0);
    CHECK_EQ(mmio_r32(TILE_LAYER_CONTROL(2)) & LAYER_LINE_SCROLL, 0u);
    tiles_layer_off(2);
    CHECK_EQ(mmio_r32(TILE_LAYER_CONTROL(2)) & LAYER_ON, 0u);
    tiles_layer(7, LAYER_ON, map, map);                      // no layer 7: nothing happens
    tiles_set_palette((void*)0xA0003000);
    CHECK_EQ(mmio_r32(TILES_PALETTE_BASE), 0xA0003000u);

    TEST_SECTION("the affine layer");
    tiles_affine(LAYER_WRAP, map, map);
    CHECK_EQ(mmio_r32(AFFINE_CONTROL), LAYER_WRAP | LAYER_ON);
    tiles_affine_rotate(100, 50, 10, 20, 0, 256);             // no turn, no zoom: the identity
    CHECK_EQ(mmio_r32(AFFINE_MATRIX_AB), 0x00000100u);
    CHECK_EQ(mmio_r32(AFFINE_MATRIX_CD), 0x01000000u);
    CHECK_EQ((int)mmio_r32(AFFINE_ORIGIN_X), (100 - 10) * 256);
    CHECK_EQ((int)mmio_r32(AFFINE_ORIGIN_Y), (50 - 20) * 256);
    tiles_affine_rotate(0, 0, 0, 0, 64, 512);                 // a quarter turn, twice as big
    CHECK_EQ(mmio_r32(AFFINE_MATRIX_AB), 0x00800000u);       // pa 0, pb 0.5
    CHECK_EQ(mmio_r32(AFFINE_MATRIX_CD), 0x0000FF80u);       // pc -0.5, pd 0
    tiles_affine_off();
    CHECK_EQ(mmio_r32(AFFINE_CONTROL) & LAYER_ON, 0u);

    TEST_SECTION("the line table");
    static const struct line_entry table[2] = { LINE_ENTRY(0, GPU_BACKGROUND, 0x112233), LINE_ENTRY(100, TILE_LAYER_SCROLL_X(1), 5) };
    CHECK_EQ(table[0].head, 0x011C0000u);
    CHECK_EQ(table[1].head, (0x360u + 0x0Cu) << 16 | 100u);
    void* lines = video_vram_alloc(sizeof table);
    tiles_load(lines, table, sizeof table);
    tiles_line_table(lines, 2);
    CHECK_EQ(mmio_r32(LINE_TABLE_BASE), (unsigned int)lines);
    CHECK_EQ(mmio_r32(LINE_TABLE_COUNT), 2u);
    video_wait_vblank();
    video_wait_vblank();                                     // a whole frame with the table
    CHECK_EQ(mmio_r32(GPU_BACKGROUND), 0x112233u);          // what it wrote stays
    CHECK_EQ(mmio_r32(TILE_LAYER_SCROLL_X(1)), 5u);
    tiles_line_table(0, 0);
    CHECK_EQ(mmio_r32(LINE_TABLE_COUNT), 0u);

    TEST_SECTION("sprites");
    CHECK_EQ(sprites_init((void*)0xA0004000, (void*)0xA0005000), 0);
    CHECK_EQ(mmio_r32(SPRITE_CONTROL), 1u);
    CHECK_EQ(mmio_r32(SPRITE_PALETTE_BASE), 0xA0004000u);
    CHECK_EQ(mmio_r32(SPRITE_TILE_BASE), 0xA0005000u);
    const struct oam_entry* oam = (const struct oam_entry*)mmio_r32(SPRITE_OAM_BASE);
    CHECK((unsigned int)oam >= VRAM_BASE);
    CHECK_EQ(sprites_limit(), 32);                           // the retro profile's
    sprite_set(3, -5, 20, SPRITE_GRAPHIC(64), SPRITE_SIZE(16, 32) | SPRITE_FLIP_Y | SPRITE_PRIORITY(2) | SPRITE_BANK(9));
    CHECK_EQ(sprite_entry(3)->position, 0x0014FFFBu);
    CHECK_EQ(sprite_entry(3)->attributes, 0x8A990002u);      // visible, priority 2, flip y, 32 high, 16 wide, bank 9, picture 2
    CHECK_EQ(oam[3].attributes, 0u);                         // not in the OAM until the commit
    sprites_commit();
    CHECK_EQ(oam[3].attributes, 0x8A990002u);
    sprite_move(3, 7, 8);
    sprite_hide(3);
    CHECK_EQ(sprite_entry(3)->position, 0x00080007u);
    CHECK_EQ(sprite_entry(3)->attributes & SPRITE_VISIBLE, 0u);
    CHECK(sprite_entry(128) == 0);
    CHECK(sprite_entry(-1) == 0);
    sprite_set(200, 0, 0, 0, 0);                             // nothing happens

    TEST_SECTION("too many sprites on a line");
    CHECK_EQ(sprites_overflow(), -1);
    sprite_hide_all();
    for (int i = 0; i < 40; i++)
        sprite_set(i, i * 6, 20 + (i & 1), 0, SPRITE_SIZE(8, 8));   // 20 on lines 20-27, 20 on 21-28: 40 from line 21
    sprites_commit();
    video_wait_vblank();
    CHECK_EQ(sprites_overflow(), 21);
    for (int i = 32; i < 40; i++)
        sprite_hide(i);                                      // down to 32: fine
    sprites_commit();
    video_wait_vblank();
    CHECK_EQ(sprites_overflow(), -1);
    CHECK_EQ(tiles_init(256, 192), 0);                       // everything off again, the sprites too
    CHECK_EQ(mmio_r32(SPRITE_CONTROL), 0u);

    TEST_SECTION("the bitmap plane alongside");
    void* before = video_vram_alloc(256);
    CHECK_EQ(fb_init(64, 32), 0);
    CHECK_EQ(video_level(), VIDEO_RETRO);                    // V2 holds the bitmap plane too
    CHECK((unsigned int)fb_pixels() > (unsigned int)before);  // its memory comes after what was taken
    fb_shutdown();
    CHECK_EQ(video_level(), VIDEO_RETRO);
    tiles_shutdown();
    CHECK_EQ(video_level(), VIDEO_TEXT);
    return test_summary();
}
