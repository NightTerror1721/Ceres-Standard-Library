#pragma once

#include "../stddef.h"

// Saved games that survive the machine stopping half way through a save. A save at `path` is two files,
// "<path>.a" and "<path>.b": each write goes to the one NOT holding the newest good copy, with a sequence number
// one past it and a CRC-32 of the bytes, so if the power goes in the middle the other file still holds the save
// before - and reading takes the newest copy whose header and CRC check out. The files go wherever fopen puts
// them: the disk ("saves/slot1", once fs_mount has run), a memory stick ("stick0:/slot1") or the host ("host:slot1").
//
//   struct progress { int level, lives, score; } me;
//   if (save_read("slot1", &version, &me, sizeof me) != sizeof me || version != 2)
//       start_new_game(&me);
//   ...
//   save_write("slot1", 2, &me, sizeof me);
//
// The version is the program's own, stored with the bytes, for telling an old layout from the current one.
// Failures are -1 (NULL) with errno: ENOENT when there is no good copy at all, ENOSPC when it does not fit in the
// buffer, EIO when a file cannot be written whole, and whatever fopen said.

#define SAVE_PATH_MAX 60                // the longest path; the files add ".a" and ".b"

int   save_write(const char* path, unsigned int version, const void* data, size_t size);
long  save_read(const char* path, unsigned int* version, void* data, size_t cap);   // the size, or -1
void* save_load(const char* path, unsigned int* version, size_t* size);            // malloc'd, or NULL
long  save_size(const char* path);        // the newest good copy's size, or -1
int   save_exists(const char* path);      // 1 when there is a good copy
int   save_erase(const char* path);       // both files; 0 when none is left
