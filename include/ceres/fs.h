#pragma once

#include "../stddef.h"

// CeresFS: a small file system on the disk (ceres/disk.h). It makes fopen() real, and can be used directly.
//
// LAYOUT (512-byte sectors):
//   sector 0            the superblock: 'CFS1', version, and where everything else is
//   next fat_sectors    the FAT, one 16-bit entry per cluster (a cluster is one sector): 0 free,
//                       0xFFFF end of file, otherwise the number of the next cluster
//   next dir_sectors    the directory, 32-byte entries: a name of up to 23 characters, the first
//                       cluster, a flag, the size in bytes (16 entries to a sector)
//   the rest            data clusters
// A disk of up to 65534 data clusters (32 MiB) is used in full. The default 64-sector disk has 60 data
// clusters (30 KiB) and room for 32 files.
//
// There are no subdirectories: a name is any text of 1..23 characters, '/' included, and it is compared
// exactly (case matters). Files are read and written in sequence and can be seeked to any place up to the
// end; there is no sparse file (seeking beyond the end is an error).
//
// Data is held in RAM and written to the disk when a file is closed, or by fs_sync(); both then call
// disk_flush(), which saves the host's image file when there is one (`ceres run --disk image.img`). There
// is no journal: a machine stopped in the middle of an update can leave the disk inconsistent. This is a
// teaching file system, not a robust one.
//
// Nothing is mounted at start. A disk that has never been formatted is all zeros: fs_mount() refuses it
// (ENODEV), and fs_format() puts a CeresFS on it. Errors are returned as -1 (or NULL) with errno set:
//   ENODEV not mounted / no CeresFS     ENOENT no such file        EEXIST the name is taken
//   ENOSPC disk or directory full       EMFILE 8 files are open    EBADF  not an open descriptor
//   EBUSY  the file is open elsewhere   EINVAL bad argument        ENAMETOOLONG a name over 23 characters
//   EIO    the disk reported an error

#define FS_NAME_MAX 23
#include "config.h"                            // FS_MAX_OPEN, unless the build sets it

#define FS_O_RDONLY 1
#define FS_O_WRONLY 2
#define FS_O_RDWR   3
#define FS_O_CREAT  4              // create the file if it does not exist
#define FS_O_TRUNC  8              // empty it if it does (needs write access)
#define FS_O_APPEND 16             // every write goes to the end (needs write access)

#define FS_SEEK_SET 0
#define FS_SEEK_CUR 1
#define FS_SEEK_END 2

struct fs_stat    { unsigned int size; unsigned int first_sector; };   // first_sector: 0 for an empty file
struct fs_dirent  { char name[24]; unsigned int size; };

// ---- the whole file system ----
int  fs_format(void);                      // erases the disk and puts a CeresFS on it, mounted; 0 ok
int  fs_mount(void);                       // 0 ok; -1 when the disk holds no CeresFS
int  fs_unmount(void);                    // closes every file, saves everything; 0 ok
int  fs_mounted(void);
void fs_sync(void);                       // writes every open file, the FAT and the directory to the disk
unsigned int fs_total_bytes(void);        // room for data (0 when not mounted)
unsigned int fs_free_bytes(void);

// ---- files, by descriptor ----
int  fs_open(const char* name, int flags);          // a descriptor >= 0, or -1
int  fs_close(int fd);
int  fs_read(int fd, void* buf, unsigned int n);    // the bytes read: fewer than n at the end, 0 at the end; -1 on error
int  fs_write(int fd, const void* buf, unsigned int n);   // the bytes written: fewer than n when the disk fills up; -1 if none could be
int  fs_seek(int fd, int offset, int whence);       // 0 ok; -1 when the place is before the start or beyond the end
int  fs_tell(int fd);
int  fs_size(int fd);

// ---- files, by name ----
int  fs_remove(const char* name);
int  fs_rename(const char* from, const char* to);
int  fs_stat(const char* name, struct fs_stat* out);
// Calls visit for every file, in directory order, until it returns nonzero. Returns how many were visited.
int  fs_list(int (*visit)(const struct fs_dirent*, void* ctx), void* ctx);
