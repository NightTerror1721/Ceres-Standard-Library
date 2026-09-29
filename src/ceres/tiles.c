// The GPU's tile layers. See ceres/tiles.h.
#include "ceres/tiles.h"
#include "ceres/fixed.h"

int tiles_init(int width, int height)
{
    if (video_set_resolution(width, height) != 0)
        return -1;
    if (video_set_level(VIDEO_RETRO) < VIDEO_RETRO)
        return -1;
    for (int layer = 0; layer < 4; ++layer)
        mmio_w32(TILE_LAYER_CONTROL(layer), 0u);
    mmio_w32(AFFINE_CONTROL, 0u);
    mmio_w32(SPRITE_CONTROL, 0u);
    mmio_w32(LINE_TABLE_COUNT, 0u);
    return 0;
}

void tiles_shutdown(void)
{
    for (int layer = 0; layer < 4; ++layer)
        mmio_w32(TILE_LAYER_CONTROL(layer), 0u);
    mmio_w32(AFFINE_CONTROL, 0u);
    mmio_w32(SPRITE_CONTROL, 0u);
    mmio_w32(LINE_TABLE_COUNT, 0u);
    video_set_level(VIDEO_TEXT);
}

int tiles_load(void* vram, const void* src, unsigned int bytes)
{
    return video_copy(vram, src, bytes);
}

void tiles_set_palette(const void* vram) { mmio_w32(TILES_PALETTE_BASE, (unsigned int)vram); }

void tiles_layer(int layer, unsigned int control, const void* map, const void* tiles)
{
    if (layer < 0 || layer > 3)
        return;
    mmio_w32(TILE_LAYER_MAP(layer), (unsigned int)map);
    mmio_w32(TILE_LAYER_TILES(layer), (unsigned int)tiles);
    mmio_w32(TILE_LAYER_CONTROL(layer), (control | LAYER_ON) & 0xFFFFu);
}

void tiles_layer_off(int layer)
{
    if (layer >= 0 && layer <= 3)
        mmio_w32(TILE_LAYER_CONTROL(layer), mmio_r32(TILE_LAYER_CONTROL(layer)) & ~LAYER_ON);
}

void tiles_scroll(int layer, int x, int y)
{
    if (layer < 0 || layer > 3)
        return;
    mmio_w32(TILE_LAYER_SCROLL_X(layer), (unsigned int)x);
    mmio_w32(TILE_LAYER_SCROLL_Y(layer), (unsigned int)y);
}

void tiles_line_scroll(int layer, const void* table)
{
    if (layer < 0 || layer > 3)
        return;
    const unsigned int control = mmio_r32(TILE_LAYER_CONTROL(layer));
    if (table == (const void*)0)
    {
        mmio_w32(TILE_LAYER_CONTROL(layer), control & ~LAYER_LINE_SCROLL);
        return;
    }
    mmio_w32(TILE_LAYER_LINE_SCROLL(layer), (unsigned int)table);
    mmio_w32(TILE_LAYER_CONTROL(layer), control | LAYER_LINE_SCROLL);
}

void tiles_affine(unsigned int control, const void* map, const void* tiles)
{
    mmio_w32(AFFINE_MAP, (unsigned int)map);
    mmio_w32(AFFINE_TILES, (unsigned int)tiles);
    mmio_w32(AFFINE_CONTROL, (control | LAYER_ON) & 0xFFFFu);
}

void tiles_affine_off(void) { mmio_w32(AFFINE_CONTROL, mmio_r32(AFFINE_CONTROL) & ~LAYER_ON); }

void tiles_affine_matrix(int ox, int oy, int pa, int pb, int pc, int pd)
{
    mmio_w32(AFFINE_ORIGIN_X, (unsigned int)ox);
    mmio_w32(AFFINE_ORIGIN_Y, (unsigned int)oy);
    mmio_w32(AFFINE_MATRIX_AB, ((unsigned int)pa & 0xFFFFu) | ((unsigned int)pb << 16));
    mmio_w32(AFFINE_MATRIX_CD, ((unsigned int)pc & 0xFFFFu) | ((unsigned int)pd << 16));
}

// An 8.8 step of the matrix, kept to what its 16 bits hold.
static int step(int value)
{
    return value > 32767 ? 32767 : value < -32768 ? -32768 : value;
}

// Map = M (screen - screen point) + map point, with M the turn by -angle divided by the zoom: what is on screen turns
// by `angle` and grows by `zoom`.
void tiles_affine_rotate(int map_x, int map_y, int screen_x, int screen_y, int angle, int zoom)
{
    if (zoom <= 0)
        zoom = 256;
    const int c = fx_cos(angle) >> 8;   // 8.8
    const int s = fx_sin(angle) >> 8;
    const int pa = step(c * 256 / zoom);
    const int pb = step(s * 256 / zoom);
    const int pc = step(-s * 256 / zoom);
    const int pd = step(c * 256 / zoom);
    tiles_affine_matrix(map_x * 256 - (pa * screen_x + pb * screen_y), map_y * 256 - (pc * screen_x + pd * screen_y), pa, pb, pc, pd);
}

void tiles_line_table(const void* vram, int entries)
{
    if (vram == (const void*)0 || entries <= 0)
    {
        mmio_w32(LINE_TABLE_COUNT, 0u);
        return;
    }
    mmio_w32(LINE_TABLE_BASE, (unsigned int)vram);
    mmio_w32(LINE_TABLE_COUNT, (unsigned int)entries);
}
