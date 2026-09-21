// CeresFS. See ceres/fs.h for the layout and the rules.
#include "ceres/fs.h"
#include "ceres/disk.h"
#include "errno.h"
#include "string.h"

#define SECTOR 512
#define ENTRY_SIZE 32
#define PER_SECTOR 16                        // directory entries in a sector
#define FAT_FREE 0
#define FAT_END 0xFFFFu
#define MAX_CLUSTERS 65534u
#define MAGIC 0x31534643u                    // 'C' 'F' 'S' '1' as little-endian bytes
#define VERSION 1u
#define ENTRY_USED 1

struct entry
{
    char name[24];
    unsigned short first;                    // first cluster; 0 for a file with no data
    unsigned short flags;
    unsigned int size;
};

// The geometry, from the superblock. Sectors are numbered from 0 on the disk; clusters from 1 in the data area.
static int mounted;
static unsigned int fat_start, fat_sectors, dir_start, dir_sectors, data_start, clusters;

// One cached FAT sector and one cached directory sector, written back when another is needed.
static unsigned int fat_buf[128];
static int fat_no;                           // which FAT sector fat_buf holds, or -1
static int fat_dirty;
static unsigned int dir_buf[128];
static int dir_no;
static int dir_dirty;
static unsigned int free_hint;               // where the next search for a free cluster starts
static int io_bad;                           // a disk transfer failed since the last check

struct ofile
{
    int used;
    int flags;
    int dir;                                 // directory entry index
    unsigned int first;                      // first cluster
    unsigned int size;
    unsigned int pos;
    unsigned int cur_index;                  // cluster number (0-based within the file) of cur_cluster
    unsigned int cur_cluster;                // 0: not known
    unsigned int buf_cluster;                // the cluster the buffer holds, 0 for none
    int buf_dirty;
    int meta_dirty;                          // size or first cluster changed
    unsigned int buf[128];
};

static struct ofile files[FS_MAX_OPEN];

// ---- the disk, with failures remembered ----

static void rd(unsigned int sector, void* buf)
{
    if (disk_read(sector, buf) != 0)
        io_bad = 1;
}

static void wr(unsigned int sector, const void* buf)
{
    if (disk_write(sector, buf) != 0)
        io_bad = 1;
}

// Ends a public operation: a failed transfer turns its result into an I/O error.
static int done(int result)
{
    if (io_bad)
    {
        io_bad = 0;
        errno = EIO;
        return -1;
    }
    return result;
}

// ---- the FAT ----

static void fat_flush(void)
{
    if (fat_no >= 0 && fat_dirty)
        wr(fat_start + (unsigned int)fat_no, fat_buf);
    fat_dirty = 0;
}

static void fat_load(unsigned int sec)
{
    if (fat_no == (int)sec)
        return;
    fat_flush();
    rd(fat_start + sec, fat_buf);
    fat_no = (int)sec;
}

static unsigned int fat_get(unsigned int c)
{
    fat_load(c * 2u / SECTOR);
    const unsigned char* b = (const unsigned char*)fat_buf;
    unsigned int at = c * 2u % SECTOR;
    return (unsigned int)b[at] | ((unsigned int)b[at + 1] << 8);
}

static void fat_set(unsigned int c, unsigned int value)
{
    fat_load(c * 2u / SECTOR);
    unsigned char* b = (unsigned char*)fat_buf;
    unsigned int at = c * 2u % SECTOR;
    b[at] = (unsigned char)(value & 255u);
    b[at + 1] = (unsigned char)(value >> 8);
    fat_dirty = 1;
}

// A free cluster, taken and marked as the end of a chain; 0 when there is none.
static unsigned int alloc_cluster(void)
{
    for (unsigned int k = 0; k < clusters; k++)
    {
        unsigned int c = (free_hint + k) % clusters + 1u;
        if (fat_get(c) == FAT_FREE)
        {
            fat_set(c, FAT_END);
            free_hint = c % clusters;
            return c;
        }
    }
    return 0;
}

static void free_chain(unsigned int first)
{
    unsigned int c = first;
    for (unsigned int guard = 0; c != 0 && c != FAT_END && c <= clusters && guard < clusters; guard++)
    {
        unsigned int next = fat_get(c);
        fat_set(c, FAT_FREE);
        c = next;
    }
}

static unsigned int count_free(void)
{
    unsigned int n = 0;
    for (unsigned int c = 1; c <= clusters; c++)
        if (fat_get(c) == FAT_FREE)
            n++;
    return n;
}

// ---- the directory ----

static void dir_flush(void)
{
    if (dir_no >= 0 && dir_dirty)
        wr(dir_start + (unsigned int)dir_no, dir_buf);
    dir_dirty = 0;
}

static void dir_load(unsigned int sec)
{
    if (dir_no == (int)sec)
        return;
    dir_flush();
    rd(dir_start + sec, dir_buf);
    dir_no = (int)sec;
}

static unsigned int entry_count(void)
{
    return dir_sectors * PER_SECTOR;
}

static void entry_get(unsigned int i, struct entry* e)
{
    dir_load(i / PER_SECTOR);
    memcpy(e, (const unsigned char*)dir_buf + (i % PER_SECTOR) * ENTRY_SIZE, ENTRY_SIZE);
}

static void entry_put(unsigned int i, const struct entry* e)
{
    dir_load(i / PER_SECTOR);
    memcpy((unsigned char*)dir_buf + (i % PER_SECTOR) * ENTRY_SIZE, e, ENTRY_SIZE);
    dir_dirty = 1;
}

// The index of the entry called `name`, or -1.
static int find_entry(const char* name, struct entry* out)
{
    for (unsigned int i = 0; i < entry_count(); i++)
    {
        struct entry e;
        entry_get(i, &e);
        if ((e.flags & ENTRY_USED) && strcmp(e.name, name) == 0)
        {
            if (out != 0)
                *out = e;
            return (int)i;
        }
    }
    return -1;
}

static int free_entry(void)
{
    for (unsigned int i = 0; i < entry_count(); i++)
    {
        struct entry e;
        entry_get(i, &e);
        if (!(e.flags & ENTRY_USED))
            return (int)i;
    }
    return -1;
}

// 0 when `name` can name a file; otherwise -1 with errno set.
static int check_name(const char* name)
{
    if (name == 0 || name[0] == 0)
    {
        errno = EINVAL;
        return -1;
    }
    if (strlen(name) > FS_NAME_MAX)
    {
        errno = ENAMETOOLONG;
        return -1;
    }
    return 0;
}

static int need_mount(void)
{
    if (!mounted)
    {
        errno = ENODEV;
        return -1;
    }
    return 0;
}

// ---- mounting ----

static void reset_state(void)
{
    fat_no = -1;
    dir_no = -1;
    fat_dirty = 0;
    dir_dirty = 0;
    free_hint = 0;
    io_bad = 0;
    memset(files, 0, sizeof files);
}

int fs_mounted(void)
{
    return mounted;
}

int fs_mount(void)
{
    if (mounted)
        fs_unmount();
    reset_state();
    unsigned int sb[128];
    rd(0, sb);
    if (io_bad)
    {
        io_bad = 0;
        errno = ENODEV;
        return -1;
    }
    unsigned int total = disk_sectors();
    if (sb[0] != MAGIC || sb[1] != VERSION || sb[2] > total || sb[3] != 1u ||
        sb[4] == 0 || sb[6] == 0 || sb[5] != sb[3] + sb[4] || sb[7] != sb[5] + sb[6] ||
        sb[8] == 0 || sb[8] > MAX_CLUSTERS || sb[7] + sb[8] > sb[2] || (sb[8] + 1u) * 2u > sb[4] * SECTOR)
    {
        errno = ENODEV;
        return -1;
    }
    fat_start = sb[3];
    fat_sectors = sb[4];
    dir_start = sb[5];
    dir_sectors = sb[6];
    data_start = sb[7];
    clusters = sb[8];
    mounted = 1;
    return 0;
}

int fs_format(void)
{
    if (mounted)
    {
        for (int i = 0; i < FS_MAX_OPEN; i++)
            if (files[i].used)
            {
                errno = EBUSY;
                return -1;
            }
        mounted = 0;
    }
    reset_state();
    unsigned int total = disk_sectors();
    if (total < 8)
    {
        errno = ENOSPC;
        return -1;
    }
    unsigned int dirs = total / 32;
    if (dirs < 1) dirs = 1;
    if (dirs > 16) dirs = 16;
    unsigned int fats = 1, n = 0;
    for (;;)
    {
        n = total - 1u - fats - dirs;
        if (n > MAX_CLUSTERS) n = MAX_CLUSTERS;
        unsigned int need = ((n + 1u) * 2u + SECTOR - 1u) / SECTOR;
        if (need <= fats)
            break;
        fats = need;
    }

    unsigned int sb[128];
    memset(sb, 0, sizeof sb);
    sb[0] = MAGIC;
    sb[1] = VERSION;
    sb[2] = total;
    sb[3] = 1u;
    sb[4] = fats;
    sb[5] = 1u + fats;
    sb[6] = dirs;
    sb[7] = 1u + fats + dirs;
    sb[8] = n;
    wr(0, sb);
    unsigned int zero[128];
    memset(zero, 0, sizeof zero);
    for (unsigned int s = 1; s < 1u + fats + dirs; s++)
        wr(s, zero);
    if (io_bad)
    {
        io_bad = 0;
        errno = EIO;
        return -1;
    }
    disk_flush();
    return fs_mount();
}

// ---- open files ----

static int valid_fd(int fd)
{
    if (need_mount() != 0)
        return 0;
    if (fd < 0 || fd >= FS_MAX_OPEN || !files[fd].used)
    {
        errno = EBADF;
        return 0;
    }
    return 1;
}

static void buf_flush(struct ofile* f)
{
    if (f->buf_dirty && f->buf_cluster != 0)
        wr(data_start + f->buf_cluster - 1u, f->buf);
    f->buf_dirty = 0;
}

// Makes the file's buffer hold cluster c. `fresh`: the cluster has no data yet, so start from zeros.
static void buf_load(struct ofile* f, unsigned int c, int fresh)
{
    if (f->buf_cluster == c)
        return;
    buf_flush(f);
    if (fresh)
        memset(f->buf, 0, sizeof f->buf);
    else
        rd(data_start + c - 1u, f->buf);
    f->buf_cluster = c;
}

// The cluster holding the `index`-th sector of the file, or 0 (with errno set) when there is none;
// with `grow` the chain is extended to reach it.
static unsigned int locate(struct ofile* f, unsigned int index, int grow)
{
    unsigned int i, c;
    if (f->first == 0)
    {
        if (!grow)
            return 0;
        c = alloc_cluster();
        if (c == 0)
        {
            errno = ENOSPC;
            return 0;
        }
        f->first = c;
        f->meta_dirty = 1;
        f->cur_index = 0;
        f->cur_cluster = c;
    }
    if (f->cur_cluster != 0 && index >= f->cur_index)
    {
        i = f->cur_index;
        c = f->cur_cluster;
    }
    else
    {
        i = 0;
        c = f->first;
    }
    while (i < index)
    {
        unsigned int next = fat_get(c);
        if (next == FAT_END)
        {
            if (!grow)
                return 0;
            next = alloc_cluster();
            if (next == 0)
            {
                errno = ENOSPC;
                return 0;
            }
            fat_set(c, next);
        }
        else if (next == 0 || next > clusters)          // a broken chain
        {
            io_bad = 1;
            return 0;
        }
        c = next;
        i++;
    }
    f->cur_index = i;
    f->cur_cluster = c;
    return c;
}

static void meta_flush(struct ofile* f)
{
    if (!f->meta_dirty)
        return;
    struct entry e;
    entry_get((unsigned int)f->dir, &e);
    e.first = (unsigned short)f->first;
    e.size = f->size;
    entry_put((unsigned int)f->dir, &e);
    f->meta_dirty = 0;
}

int fs_open(const char* name, int flags)
{
    if (need_mount() != 0 || check_name(name) != 0)
        return -1;
    if ((flags & 3) == 0 || ((flags & (FS_O_TRUNC | FS_O_APPEND)) && !(flags & FS_O_WRONLY)))
    {
        errno = EINVAL;
        return -1;
    }
    struct entry e;
    memset(&e, 0, sizeof e);                                         // stays empty when the file is new
    int at = find_entry(name, &e);
    int slot = -1;
    for (int i = 0; i < FS_MAX_OPEN; i++)
        if (!files[i].used)
        {
            slot = i;
            break;
        }

    if (at >= 0)
    {
        for (int i = 0; i < FS_MAX_OPEN; i++)                        // one writer, or any number of readers
            if (files[i].used && files[i].dir == at && ((files[i].flags | flags) & FS_O_WRONLY))
            {
                errno = EBUSY;
                return -1;
            }
    }
    else
    {
        if (!(flags & FS_O_CREAT))
        {
            errno = ENOENT;
            return -1;
        }
        at = free_entry();
        if (at < 0)
        {
            errno = ENOSPC;
            return -1;
        }
    }
    if (slot < 0)
    {
        errno = EMFILE;
        return -1;
    }

    if (!(e.flags & ENTRY_USED))                                     // a new file
    {
        memset(&e, 0, sizeof e);
        strcpy(e.name, name);
        e.flags = ENTRY_USED;
        entry_put((unsigned int)at, &e);
    }
    else if ((flags & FS_O_TRUNC) && e.first != 0)
    {
        free_chain(e.first);
        e.first = 0;
        e.size = 0;
        entry_put((unsigned int)at, &e);
    }
    else if (flags & FS_O_TRUNC)
    {
        e.size = 0;
        entry_put((unsigned int)at, &e);
    }

    struct ofile* f = &files[slot];
    memset(f, 0, sizeof *f);
    f->used = 1;
    f->flags = flags;
    f->dir = at;
    f->first = e.first;
    f->size = e.size;
    return done(slot);
}

int fs_close(int fd)
{
    if (!valid_fd(fd))
        return -1;
    struct ofile* f = &files[fd];
    buf_flush(f);
    meta_flush(f);
    fat_flush();
    dir_flush();
    f->used = 0;
    disk_flush();
    return done(0);
}

int fs_read(int fd, void* buf, unsigned int n)
{
    if (!valid_fd(fd))
        return -1;
    struct ofile* f = &files[fd];
    if (!(f->flags & FS_O_RDONLY))
    {
        errno = EBADF;
        return -1;
    }
    unsigned char* out = (unsigned char*)buf;
    unsigned int done_bytes = 0;
    if (n > f->size - f->pos && f->pos <= f->size)
        n = f->size - f->pos;
    else if (f->pos > f->size)
        n = 0;
    while (n > 0)
    {
        unsigned int index = f->pos / SECTOR, off = f->pos % SECTOR;
        unsigned int c = locate(f, index, 0);
        if (c == 0)
            break;
        buf_load(f, c, 0);
        unsigned int chunk = SECTOR - off;
        if (chunk > n)
            chunk = n;
        memcpy(out + done_bytes, (const unsigned char*)f->buf + off, chunk);
        f->pos += chunk;
        done_bytes += chunk;
        n -= chunk;
    }
    return done((int)done_bytes);
}

int fs_write(int fd, const void* buf, unsigned int n)
{
    if (!valid_fd(fd))
        return -1;
    struct ofile* f = &files[fd];
    if (!(f->flags & FS_O_WRONLY))
    {
        errno = EBADF;
        return -1;
    }
    if (f->flags & FS_O_APPEND)
        f->pos = f->size;
    const unsigned char* in = (const unsigned char*)buf;
    unsigned int written = 0;
    while (n > 0)
    {
        unsigned int index = f->pos / SECTOR, off = f->pos % SECTOR;
        unsigned int c = locate(f, index, 1);
        if (c == 0)
            break;                                       // no room (errno says so) or a broken chain
        buf_load(f, c, index * SECTOR >= f->size);        // a cluster past the old end has no data worth reading
        unsigned int chunk = SECTOR - off;
        if (chunk > n)
            chunk = n;
        memcpy((unsigned char*)f->buf + off, in + written, chunk);
        f->buf_dirty = 1;
        f->pos += chunk;
        written += chunk;
        n -= chunk;
        if (f->pos > f->size)
        {
            f->size = f->pos;
            f->meta_dirty = 1;
        }
    }
    if (written == 0 && n > 0)
        return done(-1);                                 // errno was set where it failed
    return done((int)written);
}

int fs_seek(int fd, int offset, int whence)
{
    if (!valid_fd(fd))
        return -1;
    struct ofile* f = &files[fd];
    long base;
    if (whence == FS_SEEK_SET) base = 0;
    else if (whence == FS_SEEK_CUR) base = (long)f->pos;
    else if (whence == FS_SEEK_END) base = (long)f->size;
    else
    {
        errno = EINVAL;
        return -1;
    }
    long target = base + offset;
    if (target < 0 || target > (long)f->size)
    {
        errno = EINVAL;
        return -1;
    }
    f->pos = (unsigned int)target;
    return 0;
}

int fs_tell(int fd)
{
    if (!valid_fd(fd))
        return -1;
    return (int)files[fd].pos;
}

int fs_size(int fd)
{
    if (!valid_fd(fd))
        return -1;
    return (int)files[fd].size;
}

void fs_sync(void)
{
    if (!mounted)
        return;
    for (int i = 0; i < FS_MAX_OPEN; i++)
        if (files[i].used)
        {
            buf_flush(&files[i]);
            meta_flush(&files[i]);
        }
    fat_flush();
    dir_flush();
    disk_flush();
    io_bad = 0;
}

int fs_unmount(void)
{
    if (!mounted)
        return 0;
    fs_sync();
    mounted = 0;
    reset_state();
    return 0;
}

// ---- by name ----

int fs_remove(const char* name)
{
    if (need_mount() != 0 || check_name(name) != 0)
        return -1;
    struct entry e;
    int at = find_entry(name, &e);
    if (at < 0)
    {
        errno = ENOENT;
        return -1;
    }
    for (int i = 0; i < FS_MAX_OPEN; i++)
        if (files[i].used && files[i].dir == at)
        {
            errno = EBUSY;
            return -1;
        }
    if (e.first != 0)
        free_chain(e.first);
    memset(&e, 0, sizeof e);
    entry_put((unsigned int)at, &e);
    return done(0);
}

int fs_rename(const char* from, const char* to)
{
    if (need_mount() != 0 || check_name(from) != 0 || check_name(to) != 0)
        return -1;
    struct entry e;
    int at = find_entry(from, &e);
    if (at < 0)
    {
        errno = ENOENT;
        return -1;
    }
    if (strcmp(from, to) == 0)
        return 0;
    if (find_entry(to, 0) >= 0)
    {
        errno = EEXIST;
        return -1;
    }
    memset(e.name, 0, sizeof e.name);
    strcpy(e.name, to);
    entry_put((unsigned int)at, &e);
    return done(0);
}

int fs_stat(const char* name, struct fs_stat* out)
{
    if (need_mount() != 0 || check_name(name) != 0)
        return -1;
    struct entry e;
    if (find_entry(name, &e) < 0)
    {
        errno = ENOENT;
        return -1;
    }
    if (out != 0)
    {
        out->size = e.size;
        out->first_sector = e.first == 0 ? 0u : data_start + e.first - 1u;
    }
    return done(0);
}

int fs_list(int (*visit)(const struct fs_dirent*, void* ctx), void* ctx)
{
    if (need_mount() != 0)
        return -1;
    int visited = 0;
    for (unsigned int i = 0; i < entry_count(); i++)
    {
        struct entry e;
        entry_get(i, &e);
        if (!(e.flags & ENTRY_USED))
            continue;
        struct fs_dirent d;
        memcpy(d.name, e.name, sizeof d.name);
        d.size = e.size;
        visited++;
        if (visit(&d, ctx) != 0)
            break;
    }
    return done(visited);
}

unsigned int fs_total_bytes(void)
{
    return mounted ? clusters * SECTOR : 0u;
}

unsigned int fs_free_bytes(void)
{
    return mounted ? count_free() * SECTOR : 0u;
}
