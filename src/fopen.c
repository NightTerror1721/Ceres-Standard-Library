// Files on the disk: fopen, freopen, tmpfile, remove and rename, over CeresFS (ceres/fs.h). A disk stream is
// a struct __file whose operations table points at the functions below; file.c does the rest (fgets,
// fprintf, fread, getline, ...) through it.
//
// Nothing here formats or mounts on its own: a disk with no CeresFS makes fopen fail with ENODEV, and the
// program calls fs_format() (once, the way one runs mkfs) or fs_mount() first.
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "errno.h"
#include "file_priv.h"
#include "ceres/fs.h"

// ---- the operations of a disk stream ----

static int disk_getc(struct __file* f)
{
    unsigned char c;
    int n = fs_read(f->fd, &c, 1);
    if (n < 0)
        f->error = 1;
    return n == 1 ? (int)c : -1;
}

static int disk_putc(struct __file* f, int c)
{
    unsigned char b = (unsigned char)c;
    return fs_write(f->fd, &b, 1) == 1 ? c : -1;
}

static int disk_seek(struct __file* f, int offset, int whence)
{
    return fs_seek(f->fd, offset, whence);
}

static int disk_tell(struct __file* f)
{
    return fs_tell(f->fd);
}

static int disk_close(struct __file* f)
{
    int result = fs_close(f->fd);
    if (f->temporary)
        fs_remove(f->name);
    return result;
}

static int disk_flush(struct __file* f)
{
    fs_sync();
    return 0;
}

static const struct __file_ops disk_ops = { disk_getc, disk_putc, disk_seek, disk_tell, disk_close, disk_flush };

// ---- the end of the program ----

// A disk file is buffered by CeresFS, and the program may end without closing it. C closes every open stream at
// exit; the first fopen registers this with atexit (and with fflush(NULL)), so the data and the sizes reach the
// disk whichever way the program ends by exit() or a return from main.
static int registered;

static void sync_all(void)
{
    fs_sync();
}

static void note_open(void)
{
    if (!registered)
    {
        registered = 1;
        atexit(sync_all);
        __file_flush_all_hook = sync_all;
    }
}

// ---- opening ----

// The CeresFS flags a mode asks for.
static int mode_flags(int readable, int writable, int truncate, int append, int create)
{
    int flags = (readable ? FS_O_RDONLY : 0) | (writable ? FS_O_WRONLY : 0);
    if (create)
        flags |= FS_O_CREAT;
    if (truncate)
        flags |= FS_O_TRUNC;
    if (append)
        flags |= FS_O_APPEND;
    return flags;
}

// Fills `f` as a stream on the open descriptor `fd`.
static void init_disk_file(struct __file* f, int fd, int readable, int writable, int append)
{
    memset(f, 0, sizeof(struct __file));
    f->kind = FILE_DISK;
    f->readable = readable;
    f->writable = writable;
    f->unget = -1;
    f->owned = 1;
    f->ops = &disk_ops;
    f->fd = fd;
    f->append = append;
}

FILE* fopen(const char* path, const char* mode)
{
    int readable, writable, truncate, append;
    if (path == 0 || __file_parse_mode(mode, &readable, &writable, &truncate, &append) != 0)
    {
        errno = EINVAL;
        return 0;
    }
    int create = mode[0] != 'r';                          // "w" and "a" create the file; "r" needs it to exist
    int fd = fs_open(path, mode_flags(readable, writable, truncate, append, create));
    if (fd < 0)
        return 0;                                         // errno says why
    struct __file* f = (struct __file*)malloc(sizeof(struct __file));
    if (f == 0)
    {
        fs_close(fd);
        errno = ENOMEM;
        return 0;
    }
    init_disk_file(f, fd, readable, writable, append);
    note_open();
    return f;
}

FILE* freopen(const char* path, const char* mode, FILE* f)
{
    if (f == 0 || f->kind != FILE_DISK)
    {
        errno = ENOSYS;                                   // only a disk stream can be reopened
        return 0;
    }
    int readable, writable, truncate, append;
    if (path == 0 || __file_parse_mode(mode, &readable, &writable, &truncate, &append) != 0)
    {
        errno = EINVAL;
        return 0;
    }
    f->ops->close(f);                                     // the old file is finished with, whatever comes next
    int fd = fs_open(path, mode_flags(readable, writable, truncate, append, mode[0] != 'r'));
    if (fd < 0)
    {
        free(f);
        return 0;
    }
    init_disk_file(f, fd, readable, writable, append);
    note_open();
    return f;
}

FILE* tmpfile(void)
{
    static unsigned int counter;
    char name[24];
    for (int tries = 0; tries < 1000; tries++)
    {
        counter++;
        snprintf(name, sizeof name, "~tmp%u", counter);
        struct fs_stat st;
        if (fs_stat(name, &st) == 0)
            continue;                                     // taken: try the next number
        int fd = fs_open(name, FS_O_RDWR | FS_O_CREAT | FS_O_TRUNC);
        if (fd < 0)
            return 0;
        struct __file* f = (struct __file*)malloc(sizeof(struct __file));
        if (f == 0)
        {
            fs_close(fd);
            fs_remove(name);
            errno = ENOMEM;
            return 0;
        }
        init_disk_file(f, fd, 1, 1, 0);
        f->temporary = 1;
        strcpy(f->name, name);
        note_open();
        return f;
    }
    errno = EEXIST;
    return 0;
}

int remove(const char* path)
{
    return fs_remove(path);
}

int rename(const char* from, const char* to)
{
    return fs_rename(from, to);
}
