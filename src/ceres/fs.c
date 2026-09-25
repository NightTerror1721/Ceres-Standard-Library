// CeresFS. See ceres/fs.h for the layout and the rules.
//
// Every volume keeps its geometry, one cached FAT sector and one cached directory sector (by absolute sector: the
// root area and the directories below it share it). Open files are one table for all volumes; each holds one data
// cluster. A version 1 volume is the flat directory of old; a version 2 volume walks paths through directories
// that are cluster chains of 64-byte entries.
#include "ceres/fs.h"
#include "ceres/timer.h"
#include "ceres/periph.h"
#include "errno.h"
#include "string.h"
#include "stdlib.h"

#define SECTOR 512
#define FAT_FREE 0
#define FAT_END 0xFFFFu
#define MAX_CLUSTERS 65534u
#define MAGIC_V1 0x31534643u                 // 'C' 'F' 'S' '1' as little-endian bytes
#define MAGIC_V2 0x32534643u                 // 'C' 'F' 'S' '2'
#define E_USED 1u
#define E_DIR 2u
#define MAX_PARTS 32                         // components of a path

struct entry
{
    char name[48];
    unsigned int first;                      // first cluster; 0 for a file with no data
    unsigned int size;
    unsigned int mtime;
    unsigned int flags;
};

struct volume
{
    char point[FS_POINT_MAX + 1];
    struct blockdev dev;
    int mounted;
    int version;
    unsigned int fat_start, fat_sectors, dir_start, dir_sectors, data_start, clusters;
    unsigned int fat_buf[128];
    int fat_no;                              // which FAT sector fat_buf holds, or -1
    int fat_dirty;
    unsigned int dir_buf[128];
    unsigned int dir_sec;                    // the absolute sector dir_buf holds, 0 for none
    int dir_dirty;
    unsigned int free_hint;                  // where the next search for a free cluster starts
    int io_bad;                              // a device transfer failed since the last check
};

static struct volume volumes[FS_MAX_VOLUMES];   // [0] is the disk's

struct ofile
{
    int used;
    struct volume* v;
    int flags;
    unsigned int dir;                        // the directory holding its entry (0: the root)
    unsigned int index;                      // and the entry's place in it
    unsigned int first;                      // first cluster
    unsigned int size;
    unsigned int pos;
    unsigned int cur_index;                  // cluster number (0-based within the file) of cur_cluster
    unsigned int cur_cluster;                // 0: not known
    unsigned int buf_cluster;                // the cluster the buffer holds, 0 for none
    int buf_dirty;
    int meta_dirty;                          // size or first cluster changed
    int modified;                            // written to: its time changes
    unsigned int buf[128];
};

static struct ofile files[FS_MAX_OPEN];

// ---- the device, with failures remembered ----

static void rd(struct volume* v, unsigned int sector, void* buf)
{
    if (v->dev.read(v->dev.ctx, sector, buf) != 0)
        v->io_bad = 1;
}

static void wr(struct volume* v, unsigned int sector, const void* buf)
{
    if (v->dev.read_only || v->dev.write(v->dev.ctx, sector, buf) != 0)
        v->io_bad = 1;
}

static void dev_flush(struct volume* v)
{
    if (v->dev.flush != 0 && !v->dev.read_only && v->dev.flush(v->dev.ctx) != 0)
        v->io_bad = 1;
}

// Ends a public operation: a failed transfer turns its result into an I/O error.
static int done(struct volume* v, int result)
{
    if (v != 0 && v->io_bad)
    {
        v->io_bad = 0;
        errno = EIO;
        return -1;
    }
    return result;
}

static unsigned int get32(const unsigned char* p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

static void put32(unsigned char* p, unsigned int x)
{
    p[0] = (unsigned char)x;
    p[1] = (unsigned char)(x >> 8);
    p[2] = (unsigned char)(x >> 16);
    p[3] = (unsigned char)(x >> 24);
}

static unsigned int now(void)
{
    return timer_clock();
}

// ---- the FAT ----

static void fat_flush(struct volume* v)
{
    if (v->fat_no >= 0 && v->fat_dirty)
        wr(v, v->fat_start + (unsigned int)v->fat_no, v->fat_buf);
    v->fat_dirty = 0;
}

static void fat_load(struct volume* v, unsigned int sec)
{
    if (v->fat_no == (int)sec)
        return;
    fat_flush(v);
    rd(v, v->fat_start + sec, v->fat_buf);
    v->fat_no = (int)sec;
}

static unsigned int fat_get(struct volume* v, unsigned int c)
{
    fat_load(v, c * 2u / SECTOR);
    const unsigned char* b = (const unsigned char*)v->fat_buf;
    unsigned int at = c * 2u % SECTOR;
    return (unsigned int)b[at] | ((unsigned int)b[at + 1] << 8);
}

static void fat_set(struct volume* v, unsigned int c, unsigned int value)
{
    fat_load(v, c * 2u / SECTOR);
    unsigned char* b = (unsigned char*)v->fat_buf;
    unsigned int at = c * 2u % SECTOR;
    b[at] = (unsigned char)(value & 255u);
    b[at + 1] = (unsigned char)(value >> 8);
    v->fat_dirty = 1;
}

// A free cluster, taken and marked as the end of a chain; 0 when there is none.
static unsigned int alloc_cluster(struct volume* v)
{
    for (unsigned int k = 0; k < v->clusters; k++)
    {
        unsigned int c = (v->free_hint + k) % v->clusters + 1u;
        if (fat_get(v, c) == FAT_FREE)
        {
            fat_set(v, c, FAT_END);
            v->free_hint = c % v->clusters;
            return c;
        }
    }
    return 0;
}

static void free_chain(struct volume* v, unsigned int first)
{
    unsigned int c = first;
    for (unsigned int guard = 0; c != 0 && c != FAT_END && c <= v->clusters && guard < v->clusters; guard++)
    {
        unsigned int next = fat_get(v, c);
        fat_set(v, c, FAT_FREE);
        c = next;
    }
}

static unsigned int count_free(struct volume* v)
{
    unsigned int n = 0;
    for (unsigned int c = 1; c <= v->clusters; c++)
        if (fat_get(v, c) == FAT_FREE)
            n++;
    return n;
}

static unsigned int cluster_sector(struct volume* v, unsigned int c)
{
    return v->data_start + c - 1u;
}

// ---- directories ----

static unsigned int entry_size(struct volume* v) { return v->version == 1 ? 32u : 64u; }
static unsigned int per_sector(struct volume* v) { return SECTOR / entry_size(v); }

static void dir_flush(struct volume* v)
{
    if (v->dir_sec != 0 && v->dir_dirty)
        wr(v, v->dir_sec, v->dir_buf);
    v->dir_dirty = 0;
}

static void dir_load(struct volume* v, unsigned int sector)
{
    if (v->dir_sec == sector)
        return;
    dir_flush(v);
    rd(v, sector, v->dir_buf);
    v->dir_sec = sector;
}

// A zeroed cluster, for a new directory or one that grows: an entry of all zeros is an empty one.
static void zero_cluster(struct volume* v, unsigned int c)
{
    unsigned int sector = cluster_sector(v, c);
    if (v->dir_sec == sector)
        v->dir_sec = 0;                      // a stale copy must not be written back over it
    unsigned int zero[128];
    memset(zero, 0, sizeof zero);
    wr(v, sector, zero);
}

// The absolute sector of the `s`-th sector of directory `dir` (0: the root area), or 0 past its end. With `grow`, a
// directory below the root gets a new, empty cluster to reach it (0 with ENOSPC when there is none).
static unsigned int dir_sector(struct volume* v, unsigned int dir, unsigned int s, int grow)
{
    if (dir == 0)
        return s < v->dir_sectors ? v->dir_start + s : 0u;
    unsigned int c = dir;
    for (unsigned int i = 0; i < s; i++)
    {
        unsigned int next = fat_get(v, c);
        if (next == FAT_END)
        {
            if (!grow)
                return 0;
            next = alloc_cluster(v);
            if (next == 0)
            {
                errno = ENOSPC;
                return 0;
            }
            zero_cluster(v, next);
            fat_set(v, c, next);
        }
        else if (next == FAT_FREE || next > v->clusters)   // a broken chain
        {
            v->io_bad = 1;
            return 0;
        }
        c = next;
    }
    return cluster_sector(v, c);
}

static void entry_decode(struct volume* v, const unsigned char* raw, struct entry* e)
{
    memset(e, 0, sizeof *e);
    if (v->version == 1)
    {
        memcpy(e->name, raw, 24);
        e->name[23] = 0;
        e->first = (unsigned int)raw[24] | ((unsigned int)raw[25] << 8);
        e->flags = ((unsigned int)raw[26] | ((unsigned int)raw[27] << 8)) & E_USED;
        e->size = get32(raw + 28);
    }
    else
    {
        memcpy(e->name, raw, 48);
        e->name[47] = 0;
        e->first = get32(raw + 48);
        e->size = get32(raw + 52);
        e->mtime = get32(raw + 56);
        e->flags = get32(raw + 60);
    }
}

static void entry_encode(struct volume* v, const struct entry* e, unsigned char* raw)
{
    if (v->version == 1)
    {
        memset(raw, 0, 32);
        memcpy(raw, e->name, 24);
        raw[23] = 0;
        raw[24] = (unsigned char)e->first;
        raw[25] = (unsigned char)(e->first >> 8);
        raw[26] = (unsigned char)(e->flags & E_USED);
        raw[27] = 0;
        put32(raw + 28, e->size);
    }
    else
    {
        memset(raw, 0, 64);
        memcpy(raw, e->name, 48);
        raw[47] = 0;
        put32(raw + 48, e->first);
        put32(raw + 52, e->size);
        put32(raw + 56, e->mtime);
        put32(raw + 60, e->flags);
    }
}

// Entry `index` of directory `dir`: 0, or -1 past the directory's end.
static int entry_get(struct volume* v, unsigned int dir, unsigned int index, struct entry* e)
{
    unsigned int sector = dir_sector(v, dir, index / per_sector(v), 0);
    if (sector == 0)
        return -1;
    dir_load(v, sector);
    entry_decode(v, (const unsigned char*)v->dir_buf + (index % per_sector(v)) * entry_size(v), e);
    return 0;
}

static void entry_put(struct volume* v, unsigned int dir, unsigned int index, const struct entry* e)
{
    unsigned int sector = dir_sector(v, dir, index / per_sector(v), 0);
    if (sector == 0)
        return;
    dir_load(v, sector);
    entry_encode(v, e, (unsigned char*)v->dir_buf + (index % per_sector(v)) * entry_size(v));
    v->dir_dirty = 1;
}

// The index of the entry called `name` in directory `dir`, or -1.
static int find_in(struct volume* v, unsigned int dir, const char* name, struct entry* out)
{
    struct entry e;
    for (unsigned int i = 0; entry_get(v, dir, i, &e) == 0; i++)
    {
        if ((e.flags & E_USED) && strcmp(e.name, name) == 0)
        {
            if (out != 0)
                *out = e;
            return (int)i;
        }
    }
    return -1;
}

// A free entry in directory `dir`; a directory below the root grows to make one. -1 with ENOSPC when there is none.
static int free_slot(struct volume* v, unsigned int dir)
{
    struct entry e;
    unsigned int i = 0;
    for (; entry_get(v, dir, i, &e) == 0; i++)
        if (!(e.flags & E_USED))
            return (int)i;
    if (dir != 0 && dir_sector(v, dir, i / per_sector(v), 1) != 0)
        return (int)i;                       // the first entry of the new cluster
    errno = ENOSPC;
    return -1;
}

// Whether directory `first` holds anything.
static int dir_empty(struct volume* v, unsigned int first)
{
    struct entry e;
    for (unsigned int i = 0; entry_get(v, first, i, &e) == 0; i++)
        if (e.flags & E_USED)
            return 0;
    return 1;
}

// ---- volumes and paths ----

static struct volume* volume_named(const char* point, size_t length)
{
    for (int i = 0; i < FS_MAX_VOLUMES; i++)
        if (volumes[i].mounted && strlen(volumes[i].point) == length && strncmp(volumes[i].point, point, length) == 0)
            return &volumes[i];
    return 0;
}

// Where a path leads: its volume, the directory that holds (or would hold) its last part, and that part's name.
// A path that names a root has is_root set. The directories walked through are kept, for a rename that must not
// move a directory into itself.
struct where
{
    struct volume* v;
    unsigned int dir;
    char name[48];
    int is_root;
    unsigned int through[MAX_PARTS];
    int depth;
};

static int resolve(const char* path, struct where* w)
{
    if (path == 0)
    {
        errno = EINVAL;
        return -1;
    }
    memset(w, 0, sizeof *w);
    const char* rest = path;
    const char* colon = strchr(path, ':');
    const char* slash = strchr(path, '/');
    if (colon != 0 && (slash == 0 || colon < slash))
    {
        w->v = volume_named(path, (size_t)(colon - path));
        rest = colon + 1;
    }
    else
    {
        w->v = volumes[0].mounted ? &volumes[0] : 0;
    }
    if (w->v == 0)
    {
        errno = ENODEV;
        return -1;
    }
    struct volume* v = w->v;

    if (v->version == 1)
    {
        // Flat: the name is the rest, '/' and all (after the one that follows a volume's name).
        if (rest != path && rest[0] == '/')
            rest++;
        if (rest[0] == 0)
        {
            if (rest == path)
            {
                errno = EINVAL;              // "" names nothing on a flat disk
                return -1;
            }
            w->is_root = 1;
            return 0;
        }
        if (strlen(rest) > FS_NAME_MAX_V1)
        {
            errno = ENAMETOOLONG;
            return -1;
        }
        strcpy(w->name, rest);
        return 0;
    }

    // The parts, with "." dropped and ".." taking the one before it away.
    const char* parts[MAX_PARTS];
    size_t lengths[MAX_PARTS];
    int count = 0;
    const char* p = rest;
    while (*p != 0)
    {
        while (*p == '/')
            p++;
        if (*p == 0)
            break;
        const char* start = p;
        while (*p != 0 && *p != '/')
            p++;
        size_t length = (size_t)(p - start);
        if (length == 1 && start[0] == '.')
            continue;
        if (length == 2 && start[0] == '.' && start[1] == '.')
        {
            if (count > 0)
                count--;
            continue;
        }
        if (length > FS_NAME_MAX)
        {
            errno = ENAMETOOLONG;
            return -1;
        }
        if (count == MAX_PARTS)
        {
            errno = ENAMETOOLONG;
            return -1;
        }
        parts[count] = start;
        lengths[count] = length;
        count++;
    }
    if (count == 0)
    {
        if (rest == path && rest[0] == 0)
        {
            errno = EINVAL;
            return -1;
        }
        w->is_root = 1;
        return 0;
    }
    unsigned int dir = 0;
    char name[48];
    for (int i = 0; i < count - 1; i++)
    {
        memcpy(name, parts[i], lengths[i]);
        name[lengths[i]] = 0;
        struct entry e;
        if (find_in(v, dir, name, &e) < 0)
        {
            errno = v->io_bad ? EIO : ENOENT;
            v->io_bad = 0;
            return -1;
        }
        if (!(e.flags & E_DIR))
        {
            errno = ENOTDIR;
            return -1;
        }
        dir = e.first;
        w->through[w->depth++] = dir;
    }
    w->dir = dir;
    memcpy(w->name, parts[count - 1], lengths[count - 1]);
    w->name[lengths[count - 1]] = 0;
    return 0;
}

static int writable(struct volume* v)
{
    if (v->dev.read_only)
    {
        errno = EROFS;
        return 0;
    }
    return 1;
}

// ---- mounting ----

static void reset_volume(struct volume* v)
{
    v->fat_no = -1;
    v->dir_sec = 0;
    v->fat_dirty = 0;
    v->dir_dirty = 0;
    v->free_hint = 0;
    v->io_bad = 0;
}

static void close_files_of(struct volume* v)
{
    for (int i = 0; i < FS_MAX_OPEN; i++)
        if (files[i].used && files[i].v == v)
            files[i].used = 0;
}

static int has_open_files(struct volume* v)
{
    for (int i = 0; i < FS_MAX_OPEN; i++)
        if (files[i].used && files[i].v == v)
            return 1;
    return 0;
}

static void buf_flush(struct ofile* f);
static void meta_flush(struct ofile* f);

static void sync_volume(struct volume* v)
{
    if (!v->mounted)
        return;
    for (int i = 0; i < FS_MAX_OPEN; i++)
        if (files[i].used && files[i].v == v)
        {
            buf_flush(&files[i]);
            meta_flush(&files[i]);
        }
    fat_flush(v);
    dir_flush(v);
    dev_flush(v);
}

static int unmount_volume(struct volume* v)
{
    if (!v->mounted)
        return 0;
    sync_volume(v);
    int failed = v->io_bad;
    close_files_of(v);
    v->mounted = 0;
    reset_volume(v);
    if (failed)
    {
        errno = EIO;
        return -1;
    }
    return 0;
}

// Reads a device's superblock into `v` and mounts it as `point`.
static int mount_into(struct volume* v, const char* point, const struct blockdev* dev)
{
    if (v->mounted)
        unmount_volume(v);
    memset(v, 0, sizeof *v);
    v->dev = *dev;
    reset_volume(v);
    unsigned int sb[128];
    rd(v, 0, sb);
    if (v->io_bad)
    {
        v->io_bad = 0;
        errno = ENODEV;
        return -1;
    }
    unsigned int total = dev->sectors(dev->ctx);
    unsigned int version = sb[0] == MAGIC_V1 ? 1u : sb[0] == MAGIC_V2 ? 2u : 0u;
    if (version == 0 || sb[1] != version || sb[2] > total || sb[3] != 1u ||
        sb[4] == 0 || sb[6] == 0 || sb[5] != sb[3] + sb[4] || sb[7] != sb[5] + sb[6] ||
        sb[8] == 0 || sb[8] > MAX_CLUSTERS || sb[7] + sb[8] > sb[2] || (sb[8] + 1u) * 2u > sb[4] * SECTOR)
    {
        errno = ENODEV;
        return -1;
    }
    v->version = (int)version;
    v->fat_start = sb[3];
    v->fat_sectors = sb[4];
    v->dir_start = sb[5];
    v->dir_sectors = sb[6];
    v->data_start = sb[7];
    v->clusters = sb[8];
    strcpy(v->point, point);
    v->mounted = 1;
    return 0;
}

static int valid_point(const char* point)
{
    if (point == 0 || point[0] == 0 || strlen(point) > FS_POINT_MAX || strchr(point, ':') != 0 || strchr(point, '/') != 0)
    {
        errno = EINVAL;
        return 0;
    }
    return 1;
}

int fs_mount_device(const char* point, struct blockdev* dev)
{
    if (!valid_point(point) || dev == 0)
    {
        errno = EINVAL;
        return -1;
    }
    struct volume* v = 0;
    if (strcmp(point, "disk") == 0)
        v = &volumes[0];
    else
    {
        v = volume_named(point, strlen(point));
        if (v != 0 && has_open_files(v))
        {
            errno = EBUSY;
            return -1;
        }
        for (int i = 1; v == 0 && i < FS_MAX_VOLUMES; i++)
            if (!volumes[i].mounted)
                v = &volumes[i];
        if (v == 0)
        {
            errno = ENFILE;                  // every volume slot is taken
            return -1;
        }
    }
    return mount_into(v, point, dev);
}

int fs_mount(void)
{
    struct blockdev dev;
    blockdev_disk(&dev);
    return fs_mount_device("disk", &dev);
}

int fs_mount_port(int port)
{
    struct blockdev dev;
    if (blockdev_port(port, &dev) != 0)
    {
        errno = ENODEV;
        return -1;
    }
    char point[8];
    strcpy(point, periph_type(port) == PERIPH_CARTRIDGE ? "cart0" : "stick0");
    point[strlen(point) - 1] = (char)('0' + port);
    return fs_mount_device(point, &dev);
}

int fs_unmount_point(const char* point)
{
    struct volume* v = point != 0 ? volume_named(point, strlen(point)) : 0;
    return v == 0 ? 0 : unmount_volume(v);
}

int fs_unmount(void)
{
    return unmount_volume(&volumes[0]);
}

int fs_mounted(void)
{
    return volumes[0].mounted;
}

int fs_is_mounted(const char* point)
{
    return point != 0 && volume_named(point, strlen(point)) != 0;
}

int fs_version(const char* point)
{
    struct volume* v = point != 0 ? volume_named(point, strlen(point)) : 0;
    return v == 0 ? -1 : v->version;
}

int fs_format_device(struct blockdev* dev, int version)
{
    if (dev == 0 || (version != 1 && version != 2))
    {
        errno = EINVAL;
        return -1;
    }
    if (dev->read_only)
    {
        errno = EROFS;
        return -1;
    }
    unsigned int total = dev->sectors(dev->ctx);
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
    sb[0] = version == 1 ? MAGIC_V1 : MAGIC_V2;
    sb[1] = (unsigned int)version;
    sb[2] = total;
    sb[3] = 1u;
    sb[4] = fats;
    sb[5] = 1u + fats;
    sb[6] = dirs;
    sb[7] = 1u + fats + dirs;
    sb[8] = n;
    int bad = dev->write(dev->ctx, 0, sb) != 0;
    unsigned int zero[128];
    memset(zero, 0, sizeof zero);
    for (unsigned int s = 1; s < 1u + fats + dirs; s++)
        bad |= dev->write(dev->ctx, s, zero) != 0;
    if (dev->flush != 0)
        bad |= dev->flush(dev->ctx) != 0;
    if (bad)
    {
        errno = EIO;
        return -1;
    }
    return 0;
}

int fs_format_version(int version)
{
    if (volumes[0].mounted)
    {
        if (has_open_files(&volumes[0]))
        {
            errno = EBUSY;
            return -1;
        }
        volumes[0].mounted = 0;              // what it held is being erased: nothing to save
        reset_volume(&volumes[0]);
    }
    struct blockdev dev;
    blockdev_disk(&dev);
    if (fs_format_device(&dev, version) != 0)
        return -1;
    return fs_mount_device("disk", &dev);
}

int fs_format(void)
{
    return fs_format_version(2);
}

void fs_sync(void)
{
    for (int i = 0; i < FS_MAX_VOLUMES; i++)
    {
        sync_volume(&volumes[i]);
        volumes[i].io_bad = 0;
    }
}

static int space_of(struct volume* v, unsigned int* total, unsigned int* free_bytes)
{
    if (total != 0)
        *total = v != 0 ? v->clusters * SECTOR : 0u;
    if (free_bytes != 0)
        *free_bytes = v != 0 ? count_free(v) * SECTOR : 0u;
    return v != 0 ? 0 : -1;
}

int fs_space(const char* point, unsigned int* total, unsigned int* free_bytes)
{
    struct volume* v = point != 0 ? volume_named(point, strlen(point)) : 0;
    if (v == 0)
        errno = ENODEV;
    return space_of(v, total, free_bytes);
}

unsigned int fs_total_bytes(void)
{
    return volumes[0].mounted ? volumes[0].clusters * SECTOR : 0u;
}

unsigned int fs_free_bytes(void)
{
    return volumes[0].mounted ? count_free(&volumes[0]) * SECTOR : 0u;
}

// ---- open files ----

static int valid_fd(int fd)
{
    if (fd < 0 || fd >= FS_MAX_OPEN || !files[fd].used || !files[fd].v->mounted)
    {
        errno = EBADF;
        return 0;
    }
    return 1;
}

static void buf_flush(struct ofile* f)
{
    if (f->buf_dirty && f->buf_cluster != 0)
        wr(f->v, cluster_sector(f->v, f->buf_cluster), f->buf);
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
        rd(f->v, cluster_sector(f->v, c), f->buf);
    f->buf_cluster = c;
}

// The cluster holding the `index`-th sector of the file, or 0 (with errno set) when there is none;
// with `grow` the chain is extended to reach it.
static unsigned int locate(struct ofile* f, unsigned int index, int grow)
{
    struct volume* v = f->v;
    unsigned int i, c;
    if (f->first == 0)
    {
        if (!grow)
            return 0;
        c = alloc_cluster(v);
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
        unsigned int next = fat_get(v, c);
        if (next == FAT_END)
        {
            if (!grow)
                return 0;
            next = alloc_cluster(v);
            if (next == 0)
            {
                errno = ENOSPC;
                return 0;
            }
            fat_set(v, c, next);
        }
        else if (next == 0 || next > v->clusters)          // a broken chain
        {
            v->io_bad = 1;
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
    if (!f->meta_dirty && !f->modified)
        return;
    struct entry e;
    if (entry_get(f->v, f->dir, f->index, &e) == 0)
    {
        e.first = f->first;
        e.size = f->size;
        if (f->modified)
            e.mtime = now();
        entry_put(f->v, f->dir, f->index, &e);
    }
    f->meta_dirty = 0;
    f->modified = 0;
}

int fs_open(const char* path, int flags)
{
    struct where w;
    if (resolve(path, &w) != 0)
        return -1;
    if (w.is_root)
    {
        errno = EISDIR;
        return -1;
    }
    struct volume* v = w.v;
    if ((flags & 3) == 0 || ((flags & (FS_O_TRUNC | FS_O_APPEND)) && !(flags & FS_O_WRONLY)))
    {
        errno = EINVAL;
        return -1;
    }
    if ((flags & (FS_O_WRONLY | FS_O_CREAT | FS_O_TRUNC)) && !writable(v))
        return -1;
    struct entry e;
    memset(&e, 0, sizeof e);                                         // stays empty when the file is new
    int at = find_in(v, w.dir, w.name, &e);
    int slot = -1;
    for (int i = 0; i < FS_MAX_OPEN; i++)
        if (!files[i].used)
        {
            slot = i;
            break;
        }

    if (at >= 0)
    {
        if (e.flags & E_DIR)
        {
            errno = EISDIR;
            return -1;
        }
        for (int i = 0; i < FS_MAX_OPEN; i++)                        // one writer, or any number of readers
            if (files[i].used && files[i].v == v && files[i].dir == w.dir && files[i].index == (unsigned int)at &&
                ((files[i].flags | flags) & FS_O_WRONLY))
            {
                errno = EBUSY;
                return -1;
            }
    }
    else
    {
        if (!(flags & FS_O_CREAT))
        {
            errno = v->io_bad ? EIO : ENOENT;
            v->io_bad = 0;
            return -1;
        }
        if (slot < 0)
        {
            errno = EMFILE;
            return -1;
        }
        at = free_slot(v, w.dir);
        if (at < 0)
            return done(v, -1);
    }
    if (slot < 0)
    {
        errno = EMFILE;
        return -1;
    }

    if (!(e.flags & E_USED))                                         // a new file
    {
        memset(&e, 0, sizeof e);
        strcpy(e.name, w.name);
        e.flags = E_USED;
        e.mtime = now();
        entry_put(v, w.dir, (unsigned int)at, &e);
    }
    else if (flags & FS_O_TRUNC)
    {
        if (e.first != 0)
            free_chain(v, e.first);
        e.first = 0;
        e.size = 0;
        e.mtime = now();
        entry_put(v, w.dir, (unsigned int)at, &e);
    }

    struct ofile* f = &files[slot];
    memset(f, 0, sizeof *f);
    f->used = 1;
    f->v = v;
    f->flags = flags;
    f->dir = w.dir;
    f->index = (unsigned int)at;
    f->first = e.first;
    f->size = e.size;
    return done(v, slot);
}

int fs_close(int fd)
{
    if (!valid_fd(fd))
        return -1;
    struct ofile* f = &files[fd];
    struct volume* v = f->v;
    buf_flush(f);
    meta_flush(f);
    fat_flush(v);
    dir_flush(v);
    f->used = 0;
    dev_flush(v);
    return done(v, 0);
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
    return done(f->v, (int)done_bytes);
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
        f->modified = 1;
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
        return done(f->v, -1);                           // errno was set where it failed
    return done(f->v, (int)written);
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

// ---- by path ----

static int open_here(struct volume* v, unsigned int dir, unsigned int index)
{
    for (int i = 0; i < FS_MAX_OPEN; i++)
        if (files[i].used && files[i].v == v && files[i].dir == dir && files[i].index == index)
            return 1;
    return 0;
}

int fs_remove(const char* path)
{
    struct where w;
    if (resolve(path, &w) != 0)
        return -1;
    if (w.is_root)
    {
        errno = EBUSY;
        return -1;
    }
    struct volume* v = w.v;
    struct entry e;
    int at = find_in(v, w.dir, w.name, &e);
    if (at < 0)
    {
        errno = v->io_bad ? EIO : ENOENT;
        v->io_bad = 0;
        return -1;
    }
    if (!writable(v))
        return -1;
    if (open_here(v, w.dir, (unsigned int)at))
    {
        errno = EBUSY;
        return -1;
    }
    if ((e.flags & E_DIR) && !dir_empty(v, e.first))
    {
        errno = ENOTEMPTY;
        return -1;
    }
    if (e.first != 0)
        free_chain(v, e.first);
    memset(&e, 0, sizeof e);
    entry_put(v, w.dir, (unsigned int)at, &e);
    return done(v, 0);
}

int fs_rename(const char* from, const char* to)
{
    struct where a, b;
    if (resolve(from, &a) != 0 || resolve(to, &b) != 0)
        return -1;
    if (a.is_root || b.is_root)
    {
        errno = EBUSY;
        return -1;
    }
    if (a.v != b.v)
    {
        errno = EXDEV;
        return -1;
    }
    struct volume* v = a.v;
    struct entry e;
    int at = find_in(v, a.dir, a.name, &e);
    if (at < 0)
    {
        errno = v->io_bad ? EIO : ENOENT;
        v->io_bad = 0;
        return -1;
    }
    if (a.dir == b.dir && strcmp(a.name, b.name) == 0)
        return 0;
    if (!writable(v))
        return -1;
    if (find_in(v, b.dir, b.name, 0) >= 0)
    {
        errno = EEXIST;
        return -1;
    }
    if (e.flags & E_DIR)
    {
        // Not into itself, nor below itself.
        for (int i = 0; i < b.depth; i++)
            if (b.through[i] == e.first)
            {
                errno = EINVAL;
                return -1;
            }
    }
    if (a.dir == b.dir)
    {
        memset(e.name, 0, sizeof e.name);
        strcpy(e.name, b.name);
        entry_put(v, a.dir, (unsigned int)at, &e);
        return done(v, 0);
    }
    int to_at = free_slot(v, b.dir);
    if (to_at < 0)
        return done(v, -1);
    struct entry moved = e;
    memset(moved.name, 0, sizeof moved.name);
    strcpy(moved.name, b.name);
    entry_put(v, b.dir, (unsigned int)to_at, &moved);
    struct entry empty;
    memset(&empty, 0, sizeof empty);
    entry_put(v, a.dir, (unsigned int)at, &empty);
    for (int i = 0; i < FS_MAX_OPEN; i++)                // an open file follows its entry
        if (files[i].used && files[i].v == v && files[i].dir == a.dir && files[i].index == (unsigned int)at)
        {
            files[i].dir = b.dir;
            files[i].index = (unsigned int)to_at;
        }
    return done(v, 0);
}

int fs_stat(const char* path, struct fs_stat* out)
{
    struct where w;
    if (resolve(path, &w) != 0)
        return -1;
    struct fs_stat st;
    memset(&st, 0, sizeof st);
    if (w.is_root)
    {
        st.is_dir = 1;
    }
    else
    {
        struct entry e;
        if (find_in(w.v, w.dir, w.name, &e) < 0)
        {
            errno = w.v->io_bad ? EIO : ENOENT;
            w.v->io_bad = 0;
            return -1;
        }
        st.size = e.size;
        st.first_sector = e.first == 0 ? 0u : cluster_sector(w.v, e.first);
        st.mtime = e.mtime;
        st.is_dir = (e.flags & E_DIR) != 0;
    }
    if (out != 0)
        *out = st;
    return done(w.v, 0);
}

int fs_mkdir(const char* path)
{
    struct where w;
    if (resolve(path, &w) != 0)
        return -1;
    struct volume* v = w.v;
    if (w.is_root)
    {
        errno = EEXIST;
        return -1;
    }
    if (v->version == 1)
    {
        errno = ENOTSUP;
        return -1;
    }
    if (!writable(v))
        return -1;
    if (find_in(v, w.dir, w.name, 0) >= 0)
    {
        errno = EEXIST;
        return -1;
    }
    int at = free_slot(v, w.dir);
    if (at < 0)
        return done(v, -1);
    unsigned int c = alloc_cluster(v);
    if (c == 0)
    {
        errno = ENOSPC;
        return -1;
    }
    zero_cluster(v, c);
    struct entry e;
    memset(&e, 0, sizeof e);
    strcpy(e.name, w.name);
    e.first = c;
    e.flags = E_USED | E_DIR;
    e.mtime = now();
    entry_put(v, w.dir, (unsigned int)at, &e);
    fat_flush(v);
    dir_flush(v);
    return done(v, 0);
}

// ---- reading directories ----

int fs_opendir(const char* path, struct fs_dir* dir)
{
    struct where w;
    if (dir == 0)
    {
        errno = EINVAL;
        return -1;
    }
    if (resolve(path, &w) != 0)
        return -1;
    memset(dir, 0, sizeof *dir);
    dir->volume = (int)(w.v - volumes);
    if (!w.is_root)
    {
        struct entry e;
        if (find_in(w.v, w.dir, w.name, &e) < 0)
        {
            errno = w.v->io_bad ? EIO : ENOENT;
            w.v->io_bad = 0;
            return -1;
        }
        if (!(e.flags & E_DIR))
        {
            errno = ENOTDIR;
            return -1;
        }
        dir->first = e.first;
    }
    return 0;
}

struct fs_dirent* fs_readdir(struct fs_dir* dir)
{
    if (dir == 0 || dir->volume < 0 || dir->volume >= FS_MAX_VOLUMES || !volumes[dir->volume].mounted)
        return 0;
    struct volume* v = &volumes[dir->volume];
    struct entry e;
    while (entry_get(v, dir->first, dir->index, &e) == 0)
    {
        dir->index++;
        if (!(e.flags & E_USED))
            continue;
        memcpy(dir->entry.name, e.name, sizeof dir->entry.name);
        dir->entry.size = e.size;
        dir->entry.mtime = e.mtime;
        dir->entry.is_dir = (e.flags & E_DIR) != 0;
        return &dir->entry;
    }
    v->io_bad = 0;
    return 0;
}

void fs_closedir(struct fs_dir* dir)
{
    if (dir != 0)
        dir->volume = -1;
}

int fs_list(int (*visit)(const struct fs_dirent*, void* ctx), void* ctx)
{
    if (!volumes[0].mounted)
    {
        errno = ENODEV;
        return -1;
    }
    struct fs_dir dir;
    memset(&dir, 0, sizeof dir);
    int visited = 0;
    struct fs_dirent* d;
    while ((d = fs_readdir(&dir)) != 0)
    {
        visited++;
        if (visit(d, ctx) != 0)
            break;
    }
    return done(&volumes[0], visited);
}

// ---- checking a volume ----

struct checker
{
    struct volume* v;
    unsigned char* marks;                    // a bit per cluster: reached from a directory
    int repair;
    struct fs_check_report* report;
};

static int marked(struct checker* k, unsigned int c)  { return (k->marks[c >> 3] >> (c & 7u)) & 1u; }
static void mark(struct checker* k, unsigned int c)   { k->marks[c >> 3] = (unsigned char)(k->marks[c >> 3] | (1u << (c & 7u))); }

// Follows a chain and marks it, stopping at the first thing wrong: a cluster out of range or already taken by
// another chain, or a link into a free cluster. With repair the chain is cut just before it (*first becomes 0
// when nothing of it is sound). Returns how many clusters it has.
static unsigned int check_chain(struct checker* k, unsigned int* first)
{
    struct volume* v = k->v;
    unsigned int c = *first, prev = 0, count = 0;
    while (c != 0 && c != FAT_END)
    {
        int broken = c > v->clusters;
        int crossed = !broken && marked(k, c);
        if (broken || crossed)
        {
            if (broken) k->report->bad_chains++;
            else k->report->cross_linked++;
            if (k->repair)
            {
                if (prev == 0) *first = 0;
                else fat_set(v, prev, FAT_END);
            }
            break;
        }
        mark(k, c);
        count++;
        unsigned int next = fat_get(v, c);
        if (next == FAT_FREE)
        {
            k->report->bad_chains++;
            if (k->repair)
                fat_set(v, c, FAT_END);
            break;
        }
        prev = c;
        c = next;
    }
    return count;
}

static void check_dir(struct checker* k, unsigned int dir, int depth)
{
    struct volume* v = k->v;
    struct entry e;
    for (unsigned int i = 0; entry_get(v, dir, i, &e) == 0; i++)
    {
        if (!(e.flags & E_USED))
            continue;
        unsigned int first = e.first;
        unsigned int count = check_chain(k, &first);
        int changed = first != e.first;
        e.first = first;
        if (e.flags & E_DIR)
        {
            k->report->directories++;
            if (e.first == 0)
            {
                k->report->bad_chains++;             // a directory with no cluster: an empty file it is
                if (k->repair)
                {
                    e.flags = E_USED;
                    e.size = 0;
                    changed = 1;
                }
            }
            else if (depth < MAX_PARTS && count > 0)
            {
                check_dir(k, e.first, depth + 1);
                entry_get(v, dir, i, &e);            // the walk below moved the cached sector
                e.first = first;
            }
        }
        else
        {
            k->report->files++;
            unsigned int need = (e.size + SECTOR - 1u) / SECTOR;
            if (count < need)
            {
                k->report->bad_sizes++;
                if (k->repair)
                {
                    e.size = count * SECTOR;
                    changed = 1;
                }
            }
            else if (count > need)
            {
                k->report->bad_sizes++;              // clusters past what the size needs
                if (k->repair)
                {
                    if (need == 0)
                    {
                        free_chain(v, e.first);
                        e.first = 0;
                    }
                    else
                    {
                        unsigned int c = e.first;
                        for (unsigned int n = 1; n < need; n++)
                            c = fat_get(v, c);
                        unsigned int rest = fat_get(v, c);
                        fat_set(v, c, FAT_END);
                        free_chain(v, rest);
                    }
                    changed = 1;
                }
            }
        }
        if (changed && k->repair)
            entry_put(v, dir, i, &e);
    }
}

int fs_check(const char* point, int repair, struct fs_check_report* out)
{
    struct volume* v = point != 0 ? volume_named(point, strlen(point)) : 0;
    if (v == 0)
    {
        errno = ENODEV;
        return -1;
    }
    if (repair && (has_open_files(v) || !writable(v)))
    {
        if (has_open_files(v))
            errno = EBUSY;
        return -1;
    }
    sync_volume(v);
    struct fs_check_report report;
    memset(&report, 0, sizeof report);
    struct checker k;
    k.v = v;
    k.repair = repair;
    k.report = &report;
    k.marks = (unsigned char*)calloc(v->clusters / 8u + 1u, 1);
    if (k.marks == 0)
    {
        errno = ENOMEM;
        return -1;
    }
    check_dir(&k, 0, 0);
    for (unsigned int c = 1; c <= v->clusters; c++)
    {
        if (marked(&k, c))
        {
            report.used_clusters++;
            continue;
        }
        if (fat_get(v, c) != FAT_FREE)
        {
            report.lost_clusters++;
            if (repair)
                fat_set(v, c, FAT_FREE);
        }
    }
    free(k.marks);
    if (repair)
        sync_volume(v);
    if (out != 0)
        *out = report;
    int problems = (int)(report.lost_clusters + report.cross_linked + report.bad_chains + report.bad_sizes);
    return done(v, problems);
}
