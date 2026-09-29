# `<ceres/sprite.h>`

The GPU's sprites (level V2, CeresASM plan/v2 SPEC 7.5): 128 small pictures, 8 to 64 pixels a side, that the GPU draws over and under the tile layers wherever the program places them, with nothing to erase or redraw. Each has a position, a picture in video memory, a size, a palette bank, flips and one of the four priorities; where two meet, the lower number is in front. (The sprites drawn by the CPU on a bitmap, struct sprite and sprite_draw, are in ceres/gfx.h.)

```c
tiles_init(256, 192);
void* pal = video_vram_alloc(1024);  tiles_load(pal, my_colours, 1024);
void* pics = video_vram_alloc(sizeof my_pictures);  tiles_load(pics, my_pictures, sizeof my_pictures);
sprites_init(pal, pics);
sprite_set(0, 100, 80, 0, SPRITE_SIZE(16, 16));
for (;;) { ...sprite_move(0, x, y)...; video_wait_vblank(); sprites_commit(); }
```

The table of sprites the GPU reads, the OAM, lives in video memory; this keeps a copy in RAM that sprite_set() and the rest change, and sprites_commit() copies it over at once with the copy engine - as the consoles did with a DMA in the vertical blank. Commit once a frame, after changing what moves.

A picture is `width x height` pixels row after row, 4 bits a pixel (two to a byte, the left one in the high half, colours from the sprite's bank of 16) or 8 (the whole palette); colour 0 is clear. Its place is given in steps of 32 bytes from the start of the pictures: SPRITE_GRAPHIC(byte offset).

A line of the screen shows no more sprites than the machine's profile allows (sprites_limit(): 16 on `micro`, 128 on `standard`); the others, in the order of their numbers, are left out of that line, and sprites_overflow() says whether that happened in the last frame.

```c
#define SPRITE_COUNT 128
```

## A sprite's attributes

```c
#define SPRITE_SIZE_CODE(n)   ((n) >= 64 ? 3u : (n) >= 32 ? 2u : (n) >= 16 ? 1u : 0u)
#define SPRITE_SIZE(w, h)     ((SPRITE_SIZE_CODE(w) << 20) | (SPRITE_SIZE_CODE(h) << 22))   // 8, 16, 32 or 64 each
#define SPRITE_BANK(b)        (((unsigned int)(b) & 15u) << 16)   // the palette bank, 4-bit pictures
#define SPRITE_FLIP_X         (1u << 24)
#define SPRITE_FLIP_Y         (1u << 25)
#define SPRITE_PRIORITY(p)    (((unsigned int)(p) & 3u) << 26)   // in front of the layers of this priority and below
#define SPRITE_8BPP           (1u << 28)
#define SPRITE_VISIBLE        (1u << 31)
#define SPRITE_GRAPHIC(offset) ((unsigned int)(offset) / 32u)

// An entry of the OAM: 16 bytes.
struct oam_entry
{
    unsigned int position;     // x in bits 15:0, y in 31:16, both signed: the top-left corner on the screen
    unsigned int attributes;   // the picture (bits 15:0) and the SPRITE_* above
    unsigned int reserved[2];  // level V3's
};

int  sprites_init(const void* palette, const void* pictures);   // the OAM in video memory, every sprite hidden, the sprites on; 0, or -1 without the memory
void sprites_off(void);
void sprite_set(int index, int x, int y, unsigned int graphic, unsigned int attributes);   // shown: SPRITE_VISIBLE is added
void sprite_move(int index, int x, int y);
void sprite_hide(int index);
void sprite_hide_all(void);
struct oam_entry* sprite_entry(int index);   // the copy in RAM, to change by hand; NULL outside 0-127
void sprites_commit(void);                   // the copy into the OAM the GPU reads
int  sprites_limit(void);                    // sprites a line
int  sprites_overflow(void);                 // the first line of the last frame with too many, or -1
```
