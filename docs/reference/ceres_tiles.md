# `<ceres/tiles.h>`

The GPU's tile layers (level V2, CeresASM plan/v2 SPEC 7.5): the screen built from small pictures, as the 8- and 16-bit consoles did it. Four layers each show a map - a grid of 16-bit entries, one a tile - over a set of tiles of 8x8 or 16x16 pixels, 4 or 8 bits a pixel, coloured through a palette of 256 entries (16 banks of 16 for 4-bit tiles). Each layer scrolls on its own, and with a table of a shift a line it waves or splits. A fifth, affine layer turns, zooms and shears its map through a 2x2 matrix. The sprites (ceres/sprite.h) go among the layers.

```c
tiles_init(320, 240);
void* palette = video_vram_alloc(1024);  tiles_load(palette, my_palette, 1024);  tiles_set_palette(palette);
void* set = video_vram_alloc(sizeof my_tiles); tiles_load(set, my_tiles, sizeof my_tiles);
void* map = video_vram_alloc(sizeof my_map);   tiles_load(map, my_map, sizeof my_map);
tiles_layer(0, LAYER_MAP(64, 32), map, set);
for (;;) { video_wait_vblank(); tiles_scroll(0, x++, 0); }
```

Everything a layer shows is in video memory: video_vram_alloc() hands it out and tiles_load() fills it, with the copy engine, which may write it at any moment - on `micro` and `pocket` the CPU's own stores reach the video memory only in the vertical blank. The registers take effect at once: a scroll written while the screen is being drawn moves the lines below. tools/img2tiles.js turns a PNG into the palette, tiles and map, in C or CASM.

LINE TABLE. A table in video memory of (line, register, value), which the GPU goes through as it draws: at the start of each line it writes the values of that line's entries, the way the CPU would. A gradient in the background colour, a split screen, a floor seen in perspective through the affine layer - without an interrupt. The GPU reads the table when a frame starts; a change made later shows from the next frame.

## Registers (the GPU's slot)

```c
#define TILES_PALETTE_BASE   (GPU_BASE + 0x300)   // RW: the layers' palette, 256 entries of 0x00RRGGBB
#define SPRITE_PALETTE_BASE  (GPU_BASE + 0x304)   // RW: the sprites' palette (ceres/sprite.h)
#define SPRITE_OAM_BASE      (GPU_BASE + 0x308)   // RW: the 128 sprites
#define SPRITE_TILE_BASE     (GPU_BASE + 0x30C)   // RW: their pictures
#define SPRITE_CONTROL       (GPU_BASE + 0x310)   // RW: bit 0 on
#define SPRITE_STATUS        (GPU_BASE + 0x314)   // R: of the last frame: bit 0 a line had too many, 31:16 the first
#define SPRITE_LIMIT         (GPU_BASE + 0x318)   // R: sprites a line, the machine's profile's
#define LINE_TABLE_BASE      (GPU_BASE + 0x31C)   // RW: the line table
#define LINE_TABLE_COUNT     (GPU_BASE + 0x320)   // RW: its entries (up to 8192), 0 off

#define TILE_LAYER(n)             (GPU_BASE + 0x340 + 0x20 * (n))   // layer n, 0-3
#define TILE_LAYER_CONTROL(n)     (TILE_LAYER(n) + 0x00)   // LAYER_*
#define TILE_LAYER_MAP(n)         (TILE_LAYER(n) + 0x04)
#define TILE_LAYER_TILES(n)       (TILE_LAYER(n) + 0x08)
#define TILE_LAYER_SCROLL_X(n)    (TILE_LAYER(n) + 0x0C)
#define TILE_LAYER_SCROLL_Y(n)    (TILE_LAYER(n) + 0x10)
#define TILE_LAYER_LINE_SCROLL(n) (TILE_LAYER(n) + 0x14)   // a word a line: dx in bits 15:0, dy in 31:16

#define AFFINE_CONTROL       (GPU_BASE + 0x3C0)   // LAYER_*, and LAYER_WRAP
#define AFFINE_MAP           (GPU_BASE + 0x3C4)
#define AFFINE_TILES         (GPU_BASE + 0x3C8)
#define AFFINE_ORIGIN_X      (GPU_BASE + 0x3CC)   // signed 24.8
#define AFFINE_ORIGIN_Y      (GPU_BASE + 0x3D0)
#define AFFINE_MATRIX_AB     (GPU_BASE + 0x3D4)   // pa in bits 15:0, pb in 31:16, signed 8.8
#define AFFINE_MATRIX_CD     (GPU_BASE + 0x3D8)   // pc, pd
```

## A layer's control

```c
#define LAYER_ON             0x0001u
#define LAYER_TILE16         0x0002u               // 16x16 tiles; 8x8 without it
#define LAYER_8BPP           0x0004u               // 8 bits a pixel, the whole palette; 4 without it
#define LAYER_PRIORITY(p)    (((unsigned int)(p) & 3u) << 4)   // 0-3, 3 in front
#define LAYER_MAP_CODE(n)    ((n) >= 128 ? 2u : (n) >= 64 ? 1u : 0u)
#define LAYER_MAP(w, h)      ((LAYER_MAP_CODE(w) << 6) | (LAYER_MAP_CODE(h) << 8))   // tiles a side: 32, 64 or 128
#define LAYER_LINE_SCROLL    0x0400u               // the table of a shift a line (tiles_line_scroll)
#define LAYER_WRAP           0x0800u               // the affine layer repeats its map; without it, outside is clear
#define LAYER_BANK(b)        (((unsigned int)(b) & 15u) << 12)   // added to the entries' palettes (4 bpp)
```

## A map's entry: 16 bits

```c
#define TILE(index, palette) ((unsigned short)(((index) & 0x3FFu) | (((unsigned int)(palette) & 7u) << 10)))
#define TILE_FLIP_X          0x2000u
#define TILE_FLIP_Y          0x4000u
#define TILE_FRONT           0x8000u               // one priority in front of its layer
```

## The line table

```c
struct line_entry
{
    unsigned int head;      // the line, and the register's offset in the GPU's slot << 16
    unsigned int value;
};
// An entry that writes `value` to `reg` (a register's address, GPU_BACKGROUND, TILE_LAYER_SCROLL_X(1)...) at `line`.
#define LINE_ENTRY(line, reg, value) { (unsigned int)(line) | (((unsigned int)(reg) - GPU_BASE) << 16), (unsigned int)(value) }

int  tiles_init(int width, int height);          // the screen at width x height in level V2, every layer, the sprites and the line table off; 0, or -1 when refused
void tiles_shutdown(void);                       // every layer off, the screen back to text
int  tiles_load(void* vram, const void* src, unsigned int bytes);   // into video memory with the copy engine; 0, or -1 outside RAM and VRAM
void tiles_set_palette(const void* vram);        // the layers' palette

void tiles_layer(int layer, unsigned int control, const void* map, const void* tiles);   // LAYER_ON is added
void tiles_layer_off(int layer);
void tiles_scroll(int layer, int x, int y);      // the map's pixel at the screen's top-left corner (the map repeats)
void tiles_line_scroll(int layer, const void* table);   // a shift a line, dx | dy << 16 (16 bits each, signed); NULL turns it off

void tiles_affine(unsigned int control, const void* map, const void* tiles);   // LAYER_ON is added
void tiles_affine_off(void);
// Screen pixel (x, y) shows map pixel ((ox + pa*x + pb*y) >> 8, (oy + pc*x + pd*y) >> 8): the origin in 24.8, the
// matrix in 8.8, all signed.
void tiles_affine_matrix(int ox, int oy, int pa, int pb, int pc, int pd);
// The map turned by `angle` (0-255 a whole turn) and zoomed by `zoom` (8.8: 256 is 1x, 512 twice as big), with map
// pixel (map_x, map_y) at screen pixel (screen_x, screen_y).
void tiles_affine_rotate(int map_x, int map_y, int screen_x, int screen_y, int angle, int zoom);

void tiles_line_table(const void* vram, int entries);   // entries of struct line_entry; NULL or 0 turns it off
```
