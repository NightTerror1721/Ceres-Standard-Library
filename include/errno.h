#pragma once

// One variable for the whole program: the machine has a single thread of control. Functions set it
// on failure and never clear it, as in C; a caller that wants to test one sets errno = 0 first.
extern int errno;

// Codes follow the usual POSIX numbering so that values printed by a Ceres program mean the
// same thing everywhere.
#define EPERM         1
#define ENOENT        2
#define EINTR         4
#define EIO           5
#define ENXIO         6
#define E2BIG         7
#define EBADF         9
#define EAGAIN        11
#define ENOMEM        12
#define EACCES        13
#define EBUSY         16
#define EEXIST        17
#define ENODEV        19
#define ENOTDIR       20
#define EISDIR        21
#define EINVAL        22
#define ENFILE        23
#define EMFILE        24
#define EFBIG         27
#define ENOSPC        28
#define ESPIPE        29
#define EROFS         30
#define EPIPE         32
#define EDOM          33
#define ERANGE        34
#define ENAMETOOLONG  36
#define ENOSYS        38
#define ENOTEMPTY     39
#define EOVERFLOW     75
#define EILSEQ        84
#define ETIMEDOUT     110
