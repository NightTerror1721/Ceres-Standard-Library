#pragma once

// Files of the host: semihosting through the machine's host file device (CeresASM 1c51ade). A program run as
// `ceres run prog.cres --host-dir <dir>` reaches the host's files under <dir> by name - levels, saves, logs,
// test data - with no disk image. Nothing outside <dir> can be named: a path is relative ("levels/1.txt"), and a
// component that is empty, "." or "..", or holds a ':' is refused with -EINVAL.
//
// Every call returns 0 or more on success and minus an errno value on failure: -ENODEV when the machine was
// started without --host-dir (or has no such device), -ENOENT, -EEXIST, -EISDIR, -ENOTEMPTY, -EMFILE (8 files
// at most), -EBADF... fopen("host:levels/1.txt", "r") gives a FILE* over the same files.

#define HOST_READ       0x01u
#define HOST_WRITE      0x02u
#define HOST_CREATE     0x04u
#define HOST_TRUNCATE   0x08u
#define HOST_APPEND     0x10u
#define HOST_EXCLUSIVE  0x20u   // with HOST_CREATE: fail with -EEXIST when it is already there

int host_available(void);                                    // 1 when a host directory is attached
int host_open(const char* path, unsigned int flags);         // a handle (0-7)
int host_close(int handle);
int host_read(int handle, void* buf, unsigned int n);        // bytes read; 0 at the end
int host_write(int handle, const void* buf, unsigned int n); // bytes written
int host_seek(int handle, int offset, int whence);           // SEEK_SET/SEEK_CUR/SEEK_END (0/1/2): the new position
int host_size(int handle);                                   // the file's size
int host_stat(const char* path);                             // a file's size; -EISDIR for a directory, -ENOENT
int host_remove(const char* path);                           // a file, or an empty directory
int host_rename(const char* from, const char* to);
int host_mkdir(const char* path);
// The index-th entry of a directory ("" is the root), sorted by name, into name[size]: its length, 0 past the
// last one. A directory's name ends in '/'. -ENAMETOOLONG when it does not fit.
int host_list(const char* dir, unsigned int index, char* name, unsigned int size);
