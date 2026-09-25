// Opening files: fopen, freopen, tmpfile, remove and rename, over CeresFS (ceres/fs.h) and, for a name that starts
// with "host:", over the host's files (ceres/hostfs.h: `ceres run --host-dir <dir>`). A stream is a struct __file
// whose operations table points at the functions below; file.c does the rest (buffering, fgets, fprintf, fread,
// getline, ...) through it.
//
// Nothing here formats or mounts on its own: a disk with no CeresFS makes fopen fail with ENODEV, and the
// program calls fs_format() (once, the way one runs mkfs) or fs_mount() first. A "host:" name on a machine started
// without --host-dir fails with ENODEV too.
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "errno.h"
#include "file_priv.h"
#include "ceres/fs.h"
#include "ceres/hostfs.h"

// ---- the operations of a disk stream ----

static int disk_read(struct __file* f, void* buf, unsigned int n)
{
    return fs_read(f->fd, buf, n);
}

static int disk_write(struct __file* f, const void* buf, unsigned int n)
{
    return fs_write(f->fd, buf, n);
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

static const struct __file_ops disk_ops = { disk_read, disk_write, disk_seek, disk_tell, disk_close, disk_flush };

// ---- the operations of a host stream ----

// A host result: the value, or -1 with errno set from the -errno the device gave.
static int host_result(int r)
{
    if (r < 0)
    {
        errno = -r;
        return -1;
    }
    return r;
}

static int host_ops_read(struct __file* f, void* buf, unsigned int n)        { return host_result(host_read(f->fd, buf, n)); }
static int host_ops_write(struct __file* f, const void* buf, unsigned int n) { return host_result(host_write(f->fd, buf, n)); }
static int host_ops_tell(struct __file* f)                                   { return host_result(host_seek(f->fd, 0, SEEK_CUR)); }
static int host_ops_close(struct __file* f)                                  { return host_result(host_close(f->fd)) < 0 ? -1 : 0; }
static int host_ops_flush(struct __file* f)                                  { return 0; }   // the device writes at once

static int host_ops_seek(struct __file* f, int offset, int whence)
{
    return host_result(host_seek(f->fd, offset, whence)) < 0 ? -1 : 0;
}

static const struct __file_ops host_ops = { host_ops_read, host_ops_write, host_ops_seek, host_ops_tell, host_ops_close, host_ops_flush };

// The host name inside "host:name", or 0 for a name on the disk.
static const char* host_name(const char* path)
{
    return strncmp(path, "host:", 5) == 0 ? path + 5 : 0;
}

// ---- the end of the program ----

// CeresFS holds data in RAM, and the program may end without closing its files. C closes every open stream at
// exit; the first fopen registers a full flush with atexit - the streams' buffers, then CeresFS to the disk (the
// hook fflush(NULL) calls) - so the data and the sizes reach the disk whichever way the program ends, by exit()
// or a return from main.
static int registered;

static void sync_disk(void)
{
    fs_sync();
}

static void flush_at_exit(void)
{
    fflush(0);
}

static void note_open(void)
{
    if (!registered)
    {
        registered = 1;
        __file_flush_all_hook = sync_disk;
        atexit(flush_at_exit);
    }
}

// ---- opening ----

// The CeresFS flags a mode asks for.
static int disk_flags(int readable, int writable, int truncate, int append, int create)
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

// Opens `path` as the stream `f`, which keeps its place on the lists and whether it is the program's to free.
// 0, or -1 with errno set (and `f` left untouched).
static int open_into(struct __file* f, const char* path, const char* mode)
{
    int readable, writable, truncate, append;
    if (path == 0 || __file_parse_mode(mode, &readable, &writable, &truncate, &append) != 0)
    {
        errno = EINVAL;
        return -1;
    }
    int create = mode[0] != 'r';                          // "w" and "a" create the file; "r" needs it to exist
    int kind;
    int fd;
    const char* host = host_name(path);
    if (host != 0)
    {
        unsigned int flags = (readable ? HOST_READ : 0) | (writable ? HOST_WRITE : 0) | (create ? HOST_CREATE : 0) |
            (truncate ? HOST_TRUNCATE : 0) | (append ? HOST_APPEND : 0);
        fd = host_result(host_open(host, flags));
        kind = FILE_HOST;
    }
    else
    {
        fd = fs_open(path, disk_flags(readable, writable, truncate, append, create));
        kind = FILE_DISK;
    }
    if (fd < 0)
        return -1;                                        // errno says why

    int owned = f->owned;
    int listed = f->listed;
    struct __file* next = f->next_open;
    memset(f, 0, sizeof(struct __file));
    f->owned = owned;
    f->listed = listed;
    f->next_open = next;
    f->kind = kind;
    f->readable = readable;
    f->writable = writable;
    f->unget = -1;
    f->ops = kind == FILE_HOST ? &host_ops : &disk_ops;
    f->fd = fd;
    f->append = append;
    f->buf_mode = _IOFBF;                                 // a buffer of FILE_BUFSIZ at the first read or write
    if (kind == FILE_DISK)
        note_open();
    return 0;
}

FILE* fopen(const char* path, const char* mode)
{
    struct __file* f = (struct __file*)malloc(sizeof(struct __file));
    if (f == 0)
    {
        errno = ENOMEM;
        return 0;
    }
    memset(f, 0, sizeof(struct __file));
    f->owned = 1;
    if (open_into(f, path, mode) != 0)
    {
        free(f);
        return 0;
    }
    return f;
}

// Closes what `f` has open and opens `path` in its place - stdin, stdout and stderr included, so a program can
// send its output to a file ("host:log.txt") or read its input from one. The name "term:" puts a standard stream
// back on the terminal (a Ceres extension). On failure the stream is closed, as C says; a standard stream is left
// on the terminal instead.
FILE* freopen(const char* path, const char* mode, FILE* f)
{
    if (f == 0 || path == 0)
    {
        errno = EINVAL;
        return 0;
    }
    int readable, writable, truncate, append;
    if (__file_parse_mode(mode, &readable, &writable, &truncate, &append) != 0)
    {
        errno = EINVAL;
        return 0;
    }
    int standard = f == stdin || f == stdout || f == stderr;
    // What f had open goes first, whatever comes next.
    __file_release_buffer(f);
    if (f->kind == FILE_DISK || f->kind == FILE_HOST)
        f->ops->close(f);
    int buf_mode = f->kind == FILE_TERM_IN || f->kind == FILE_TERM_OUT || f->kind == FILE_TERM_ERR ? f->buf_mode : _IOFBF;

    if (strcmp(path, "term:") == 0 || open_into(f, path, mode) != 0)
    {
        int failed = strcmp(path, "term:") != 0;
        int saved = errno;
        if (!standard)
        {
            __file_forget(f);
            if (f->owned)
                free(f);
            return 0;
        }
        // Back on the terminal, as it started.
        int listed = f->listed;
        struct __file* next = f->next_open;
        memset(f, 0, sizeof(struct __file));
        f->listed = listed;
        f->next_open = next;
        f->kind = f == stdin ? FILE_TERM_IN : f == stdout ? FILE_TERM_OUT : FILE_TERM_ERR;
        f->readable = f == stdin;
        f->writable = f != stdin;
        f->unget = -1;
        f->buf_mode = _IONBF;
        if (f == stdout)
            __file_stdout_changed();
        errno = saved;
        return failed ? 0 : f;
    }
    if (standard && f != stdin)
        f->buf_mode = buf_mode == _IONBF ? _IOLBF : buf_mode;   // a log file is written a line at a time
    if (f == stdout)
        __file_stdout_changed();
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
        FILE* f = fopen(name, "w+");
        if (f == 0)
            return 0;
        f->temporary = 1;
        strcpy(f->name, name);
        return f;
    }
    errno = EEXIST;
    return 0;
}

int remove(const char* path)
{
    if (path != 0 && host_name(path) != 0)
        return host_result(host_remove(host_name(path))) < 0 ? -1 : 0;
    return fs_remove(path);
}

int rename(const char* from, const char* to)
{
    if (from == 0 || to == 0)
    {
        errno = EINVAL;
        return -1;
    }
    const char* host_from = host_name(from);
    const char* host_to = host_name(to);
    if ((host_from != 0) != (host_to != 0))
    {
        errno = EXDEV;                                    // between the disk and the host: copy it instead
        return -1;
    }
    if (host_from != 0)
        return host_result(host_rename(host_from, host_to)) < 0 ? -1 : 0;
    return fs_rename(from, to);
}
