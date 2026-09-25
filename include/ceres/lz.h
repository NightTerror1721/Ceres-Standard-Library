#pragma once

#include "../stddef.h"

// LZ4: compression that is fast to undo, for assets on a cartridge, saves and anything else worth keeping small.
// Decompressing needs no memory of its own and runs at a few instructions a byte; compressing is greedy (the
// classic "fast" LZ4) and uses a 16 KiB table of the library's, so it must not be called again while it runs
// (from an interrupt handler; the tasks of ceres/task.h do not switch inside it).
//
// The BLOCK format is the raw one: sequences of literals and matches, as LZ4_decompress_safe reads. The FRAME
// format is what the `lz4` command-line tool writes (magic 0x184D2204): lz4_frame_decompress reads it, blocks
// linked or independent, compressed or stored; its checksums are skipped rather than checked. Frames may follow
// one another (and skippable frames); bytes after the last frame that are not one are left unread.
//
//   char packed[LZ4_BOUND(sizeof level)];
//   int n = lz4_compress(level, sizeof level, packed, sizeof packed);
//   ...
//   int got = lz4_decompress(packed, n, level, sizeof level);   // sizeof level, or -1
//
// Every function returns the bytes it wrote, or -1 (errno EINVAL for input that is not LZ4, ENOSPC when the output
// does not fit). The decompressors never read or write outside the buffers they are given, whatever the input.

#define LZ4_BOUND(n) ((n) + (n) / 255u + 16u)    // the most lz4_compress can write for n bytes (n is used twice)

size_t lz4_bound(size_t n);
int    lz4_compress(const void* src, size_t n, void* dst, size_t cap);
int    lz4_decompress(const void* src, size_t n, void* dst, size_t cap);
int    lz4_frame_decompress(const void* src, size_t n, void* dst, size_t cap);
// The size a frame says its content has (its optional content-size field), or -1 when it does not say.
long long lz4_frame_content_size(const void* src, size_t n);
