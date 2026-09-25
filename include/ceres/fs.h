#pragma once

#include "../stddef.h"
#include "blockdev.h"

// CeresFS: a small file system on a block device (ceres/blockdev.h): the internal disk, a memory stick or a
// cartridge in a peripheral port. It makes fopen() real, and can be used directly.
//
// VOLUMES. Several can be mounted at once, each under a name: "disk" for the internal disk, "stickN" and "cartN"
// for what is in port N (fs_mount_port), or any name up to 7 characters (fs_mount_device). A path names one with
// "name:/" - "stick0:/saves/slot1.dat", "cart1:/level1.map" - and one that does not start that way is on the
// disk, so every program written for one disk goes on working (and a name may hold a ':' not followed by '/').
// fopen() takes the same paths ("host:..." is ceres/hostfs.h).
//
// LAYOUT (512-byte sectors):
//   sector 0            the superblock: 'CFS2' (or 'CFS1'), the version, and where everything else is
//   next fat_sectors    the FAT, one 16-bit entry per cluster (a cluster is one sector): 0 free,
//                       0xFFFF end of a chain, otherwise the number of the next cluster
//   next dir_sectors    the root directory
//   the rest            data clusters: files, and (version 2) the directories below the root
// A disk of up to 65534 data clusters (32 MiB) is used in full.
//
// VERSION 2 (what fs_format() makes) has DIRECTORIES: a directory is a chain of clusters holding 64-byte entries,
// 8 to a sector - a name of up to 47 characters, the first cluster, the size, the time of the last change
// (seconds since 1970, from the machine's clock) and whether it is a directory. '/' separates the parts of a
// path; "." and ".." mean what they always do. A sector holds 8 entries: the default 64-sector disk has 2 root
// sectors, 16 names in the root; a directory below it grows a sector at a time.
//
// VERSION 1 (fs_format_version(1), and every disk formatted before) is flat: one directory of 32-byte entries,
// a name of up to 23 characters in which '/' is a character like any other, and no times. It mounts and works as
// it always did; fs_mkdir on it fails with ENOTSUP.
//
// Files are read and written in sequence and can be seeked to any place up to the end; there is no sparse file
// (seeking beyond the end is an error). Data is held in RAM and written to the device when a file is closed, or
// by fs_sync(); both then flush the device (the host's image file, `ceres run --disk image.img`, a stick's file).
// There is no journal: a machine stopped in the middle of an update can leave a volume inconsistent, and
// fs_check() finds (and with `repair` fixes) what that leaves behind.
//
// Nothing is mounted at start. A disk that has never been formatted is all zeros: fs_mount() refuses it
// (ENODEV), and fs_format() puts a CeresFS on it. Errors are returned as -1 (or NULL) with errno set:
//   ENODEV not mounted / no CeresFS     ENOENT no such file        EEXIST the name is taken
//   ENOSPC disk or directory full       EMFILE 8 files are open    EBADF  not an open descriptor
//   EBUSY  the file is open elsewhere   EINVAL bad argument        ENAMETOOLONG a name too long
//   ENOTDIR a path goes through a file  EISDIR it names a directory ENOTEMPTY a directory with files in it
//   EROFS  a cartridge                  EXDEV  a rename between volumes   ENOTSUP no directories on version 1
//   EIO    the device reported an error  ENFILE no free volume slot     ENOMEM out of memory (fs_check)

#define FS_NAME_MAX     47             // a name in a version 2 directory (23 on version 1)
#define FS_NAME_MAX_V1  23
#define FS_POINT_MAX    7              // a mount point's name
#define FS_MAX_VOLUMES  5              // the disk and four ports
#include "config.h"                    // FS_MAX_OPEN, unless the build sets it

#define FS_O_RDONLY 1
#define FS_O_WRONLY 2
#define FS_O_RDWR   3
#define FS_O_CREAT  4              // create the file if it does not exist
#define FS_O_TRUNC  8              // empty it if it does (needs write access)
#define FS_O_APPEND 16             // every write goes to the end (needs write access)

#define FS_SEEK_SET 0
#define FS_SEEK_CUR 1
#define FS_SEEK_END 2

struct fs_stat
{
    unsigned int size;
    unsigned int first_sector;         // 0 for an empty file
    unsigned int mtime;                // seconds since 1970 of the last change; 0 on version 1
    int is_dir;
};

struct fs_dirent
{
    char name[48];
    unsigned int size;
    unsigned int mtime;
    int is_dir;
};

// ---- the internal disk ----
int  fs_format(void);                      // erases the disk and puts a version 2 CeresFS on it, mounted; 0 ok
int  fs_format_version(int version);       // the same with version 1 or 2
int  fs_mount(void);                       // mounts the disk as "disk"; 0 ok, -1 when it holds no CeresFS
int  fs_unmount(void);                     // closes its files, saves everything; 0 ok
int  fs_mounted(void);
void fs_sync(void);                        // every volume: open files, FATs and directories, out to their devices
unsigned int fs_total_bytes(void);         // room for data on the disk (0 when not mounted)
unsigned int fs_free_bytes(void);

// ---- other volumes ----
int  fs_format_device(struct blockdev* dev, int version);   // puts a CeresFS on a device, not mounted
int  fs_mount_device(const char* point, struct blockdev* dev);   // mounts it as `point`
int  fs_mount_port(int port);              // what is in port N, as "stickN" or "cartN" (read only when the medium is)
int  fs_unmount_point(const char* point);  // saves and forgets it (the disk too, as "disk")
int  fs_is_mounted(const char* point);
int  fs_space(const char* point, unsigned int* total, unsigned int* free_bytes);
int  fs_version(const char* point);        // 1 or 2; -1 when nothing is mounted there

// ---- files, by descriptor ----
int  fs_open(const char* path, int flags);          // a descriptor >= 0, or -1
int  fs_close(int fd);
int  fs_read(int fd, void* buf, unsigned int n);    // the bytes read: fewer than n at the end, 0 at the end; -1 on error
int  fs_write(int fd, const void* buf, unsigned int n);   // the bytes written: fewer than n when the disk fills up; -1 if none could be
int  fs_seek(int fd, int offset, int whence);       // 0 ok; -1 when the place is before the start or beyond the end
int  fs_tell(int fd);
int  fs_size(int fd);

// ---- by path ----
int  fs_remove(const char* path);          // a file, or an empty directory
int  fs_rename(const char* from, const char* to);   // on one volume; into another directory too (version 2)
int  fs_stat(const char* path, struct fs_stat* out);
int  fs_mkdir(const char* path);           // version 2
// Calls visit for every file in the disk's root, in directory order, until it returns nonzero. Returns how many
// were visited.
int  fs_list(int (*visit)(const struct fs_dirent*, void* ctx), void* ctx);

// ---- reading a directory ----
struct fs_dir
{
    int volume;
    unsigned int first;                // the directory's first cluster; 0 for the root
    unsigned int index;                // the next entry to look at
    struct fs_dirent entry;
};
int  fs_opendir(const char* path, struct fs_dir* dir);   // 0, or -1; "/" or "stick0:/" is a root ("" is EINVAL)
struct fs_dirent* fs_readdir(struct fs_dir* dir);        // the next entry, or NULL after the last
void fs_closedir(struct fs_dir* dir);

// ---- checking a volume (fsck) ----
struct fs_check_report
{
    unsigned int files, directories;
    unsigned int used_clusters;        // reached from the directories
    unsigned int lost_clusters;        // taken in the FAT, reached from nowhere
    unsigned int cross_linked;         // a cluster two chains share
    unsigned int bad_chains;           // a chain running into a free or impossible cluster
    unsigned int bad_sizes;            // a size its chain cannot hold, or a chain longer than its size needs
};
// Walks every directory and chain of the volume and counts what is wrong; 0 when nothing is. With `repair`, lost
// clusters are freed, a broken or over-long chain is cut, a size is brought within its chain, and a chain that
// crosses another's is cut at the crossing (its entry emptied when the first cluster itself is shared); the
// volume must have no open files (EBUSY).
int  fs_check(const char* point, int repair, struct fs_check_report* out);
