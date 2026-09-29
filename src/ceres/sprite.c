// The GPU's sprites. See ceres/sprite.h.
#include "ceres/sprite.h"

static struct oam_entry shadow[SPRITE_COUNT];   // what sprites_commit() copies into the OAM
static void* oam;                                // the OAM, in video memory
static unsigned int oam_resets;                  // video_vram_resets() when it was taken

int sprites_init(const void* palette, const void* pictures)
{
    if (oam == (void*)0 || oam_resets != video_vram_resets())
    {
        oam = video_vram_alloc(sizeof shadow);
        oam_resets = video_vram_resets();
        if (oam == (void*)0)
            return -1;
    }
    sprite_hide_all();
    sprites_commit();
    mmio_w32(SPRITE_PALETTE_BASE, (unsigned int)palette);
    mmio_w32(SPRITE_TILE_BASE, (unsigned int)pictures);
    mmio_w32(SPRITE_OAM_BASE, (unsigned int)oam);
    mmio_w32(SPRITE_CONTROL, 1u);
    return 0;
}

void sprites_off(void) { mmio_w32(SPRITE_CONTROL, 0u); }

struct oam_entry* sprite_entry(int index)
{
    return index >= 0 && index < SPRITE_COUNT ? &shadow[index] : (struct oam_entry*)0;
}

static unsigned int position(int x, int y)
{
    return ((unsigned int)x & 0xFFFFu) | ((unsigned int)y << 16);
}

void sprite_set(int index, int x, int y, unsigned int graphic, unsigned int attributes)
{
    struct oam_entry* entry = sprite_entry(index);
    if (entry == (struct oam_entry*)0)
        return;
    entry->position = position(x, y);
    entry->attributes = (graphic & 0xFFFFu) | (attributes & 0xFFFF0000u) | SPRITE_VISIBLE;
}

void sprite_move(int index, int x, int y)
{
    struct oam_entry* entry = sprite_entry(index);
    if (entry != (struct oam_entry*)0)
        entry->position = position(x, y);
}

void sprite_hide(int index)
{
    struct oam_entry* entry = sprite_entry(index);
    if (entry != (struct oam_entry*)0)
        entry->attributes &= ~SPRITE_VISIBLE;
}

void sprite_hide_all(void)
{
    for (int i = 0; i < SPRITE_COUNT; ++i)
    {
        shadow[i].position = 0;
        shadow[i].attributes = 0;
        shadow[i].reserved[0] = shadow[i].reserved[1] = 0;
    }
}

void sprites_commit(void)
{
    if (oam != (void*)0)
        video_copy(oam, shadow, sizeof shadow);
}

int sprites_limit(void) { return (int)mmio_r32(SPRITE_LIMIT); }

int sprites_overflow(void)
{
    const unsigned int status = mmio_r32(SPRITE_STATUS);
    return (status & 1u) ? (int)(status >> 16) : -1;
}
