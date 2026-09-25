// CeresFS version 2 and the volumes: directories, paths, times, several devices mounted at once, and fs_check.
// A stick is in port 0 and a cartridge (not a CeresFS) in port 2 (tests/expected/test_fs_dirs.ports).
#include "ceres/test.h"
#include "ceres/fs.h"
#include "ceres/disk.h"
#include "ceres/blockdev.h"
#include "stdio.h"
#include "string.h"
#include "errno.h"

static void make(const char* path, const char* text)
{
    int fd = fs_open(path, FS_O_WRONLY | FS_O_CREAT | FS_O_TRUNC);
    fs_write(fd, text, (unsigned int)strlen(text));
    fs_close(fd);
}

static int count_entries(const char* path)
{
    struct fs_dir d;
    if (fs_opendir(path, &d) != 0)
        return -1;
    int n = 0;
    while (fs_readdir(&d) != 0)
        n++;
    fs_closedir(&d);
    return n;
}

// A device of the program's own: 64 sectors of RAM.
static unsigned char ram[64 * 512];
static int ram_read(void* ctx, unsigned int s, void* buf)        { memcpy(buf, ram + s * 512u, 512); return 0; }
static int ram_write(void* ctx, unsigned int s, const void* buf) { memcpy(ram + s * 512u, buf, 512); return 0; }
static unsigned int ram_sectors(void* ctx)                        { return 64; }

int main(void)
{
    TEST_SECTION("a version 2 disk");
    CHECK_EQ(fs_format(), 0);
    CHECK_EQ(fs_version("disk"), 2);
    struct fs_stat st;
    CHECK_EQ(fs_stat("/", &st), 0);
    CHECK_EQ(st.is_dir, 1);

    TEST_SECTION("directories and paths");
    CHECK_EQ(fs_mkdir("saves"), 0);
    CHECK_EQ(fs_mkdir("saves"), -1);
    CHECK_EQ(errno, EEXIST);
    CHECK_EQ(fs_mkdir("saves/old"), 0);
    make("saves/slot1.dat", "level=3");
    make("disk:/saves/old/slot0.dat", "level=1");        // the volume's name is optional for the disk
    CHECK_EQ(fs_stat("saves/./old/../slot1.dat", &st), 0);
    CHECK_EQ((int)st.size, 7);
    CHECK_EQ(st.is_dir, 0);
    CHECK(st.mtime > 1600000000u);                      // the machine's clock: after 2020
    CHECK_EQ(fs_stat("saves/old", &st), 0);
    CHECK_EQ(st.is_dir, 1);
    CHECK_EQ(fs_open("saves", FS_O_RDONLY), -1);
    CHECK_EQ(errno, EISDIR);
    CHECK_EQ(fs_open("saves/slot1.dat/x", FS_O_RDONLY), -1);
    CHECK_EQ(errno, ENOTDIR);
    CHECK_EQ(fs_open("nowhere/x", FS_O_RDONLY), -1);
    CHECK_EQ(errno, ENOENT);
    CHECK_EQ(fs_mkdir("a/b"), -1);                       // one level at a time
    CHECK_EQ(errno, ENOENT);
    char name47[48];
    memset(name47, 'n', 47);
    name47[47] = 0;
    make(name47, "x");
    CHECK_EQ(fs_stat(name47, &st), 0);
    char name48[49];
    memset(name48, 'n', 48);
    name48[48] = 0;
    CHECK_EQ(fs_open(name48, FS_O_WRONLY | FS_O_CREAT), -1);
    CHECK_EQ(errno, ENAMETOOLONG);

    TEST_SECTION("reading a directory");
    struct fs_dir d;
    CHECK_EQ(fs_opendir("saves", &d), 0);
    struct fs_dirent* e;
    while ((e = fs_readdir(&d)) != 0)
        printf("  %s%s %u\n", e->name, e->is_dir ? "/" : "", e->is_dir ? 0u : e->size);
    fs_closedir(&d);
    CHECK_EQ(fs_opendir("saves/slot1.dat", &d), -1);
    CHECK_EQ(errno, ENOTDIR);
    CHECK_EQ(count_entries(""), -1);                    // "" names nothing
    CHECK_EQ(count_entries("/"), 2);                    // saves and the long name

    TEST_SECTION("a directory grows past a sector");
    CHECK_EQ(fs_mkdir("many"), 0);
    char path[32];
    for (int i = 0; i < 20; i++)                        // 8 entries a sector: three clusters
    {
        snprintf(path, sizeof path, "many/file%02d", i);
        make(path, "data");
    }
    CHECK_EQ(count_entries("many"), 20);
    CHECK_EQ(fs_stat("many/file19", &st), 0);
    CHECK_EQ(fs_remove("many"), -1);
    CHECK_EQ(errno, ENOTEMPTY);
    for (int i = 0; i < 20; i++)
    {
        snprintf(path, sizeof path, "many/file%02d", i);
        CHECK_EQ(fs_remove(path), 0);
    }
    CHECK_EQ(fs_remove("many"), 0);

    TEST_SECTION("rename moves between directories");
    CHECK_EQ(fs_rename("saves/slot1.dat", "saves/old/slot1.dat"), 0);
    CHECK_EQ(fs_stat("saves/slot1.dat", &st), -1);
    CHECK_EQ(fs_stat("saves/old/slot1.dat", &st), 0);
    CHECK_EQ(fs_rename("saves", "saves/old/inside"), -1);   // not into itself
    CHECK_EQ(errno, EINVAL);
    FILE* f = fopen("saves/old/slot1.dat", "r");        // open, then moved: it follows
    CHECK_EQ(fs_rename("saves/old/slot1.dat", "moved.dat"), 0);
    char text[16] = { 0 };
    CHECK(fgets(text, sizeof text, f) != 0);
    CHECK_STR(text, "level=3");
    fclose(f);
    CHECK_EQ(fs_stat("moved.dat", &st), 0);

    TEST_SECTION("a stick in port 0");
    CHECK_EQ(fs_mount_port(0), -1);                     // nothing formatted on it yet
    CHECK_EQ(errno, ENODEV);
    struct blockdev stick;
    CHECK_EQ(blockdev_port(0, &stick), 0);
    CHECK_EQ(fs_format_device(&stick, 2), 0);
    CHECK_EQ(fs_mount_port(0), 0);
    CHECK_EQ(fs_is_mounted("stick0"), 1);
    f = fopen("stick0:/save.dat", "w");
    CHECK(f != 0);
    fputs("on the stick\n", f);
    fclose(f);
    CHECK_EQ(fs_mkdir("stick0:/levels"), 0);
    CHECK_EQ(count_entries("stick0:/"), 2);
    CHECK_EQ(fs_stat("save.dat", &st), -1);             // that is the disk's root
    CHECK_EQ(fs_rename("stick0:/save.dat", "disk:/save.dat"), -1);
    CHECK_EQ(errno, EXDEV);
    unsigned int total = 0, free_bytes = 0;
    CHECK_EQ(fs_space("stick0", &total, &free_bytes), 0);
    CHECK(total > 0 && free_bytes < total);
    CHECK_EQ(fs_unmount_point("stick0"), 0);
    CHECK_EQ(fs_is_mounted("stick0"), 0);
    CHECK(fopen("stick0:/save.dat", "r") == 0);
    CHECK_EQ(errno, ENODEV);
    CHECK_EQ(fs_mount_port(0), 0);                      // and it is all still there
    f = fopen("stick0:/save.dat", "r");
    CHECK(fgets(text, sizeof text, f) != 0);
    CHECK_STR(text, "on the stick\n");
    fclose(f);
    CHECK_EQ(fs_mount_port(2), -1);                     // the cartridge holds no CeresFS
    CHECK_EQ(fs_mount_port(3), -1);                     // an empty port

    TEST_SECTION("a read-only volume");
    struct blockdev rd = { ram_read, ram_write, ram_sectors, 0, 0, 0 };
    CHECK_EQ(fs_format_device(&rd, 2), 0);
    CHECK_EQ(fs_mount_device("ram", &rd), 0);
    make("ram:/kept", "kept");
    CHECK_EQ(fs_unmount_point("ram"), 0);
    rd.read_only = 1;
    CHECK_EQ(fs_mount_device("ram", &rd), 0);
    CHECK_EQ(fs_stat("ram:/kept", &st), 0);
    CHECK_EQ(fs_open("ram:/new", FS_O_WRONLY | FS_O_CREAT), -1);
    CHECK_EQ(errno, EROFS);
    CHECK_EQ(fs_remove("ram:/kept"), -1);
    CHECK_EQ(errno, EROFS);
    CHECK_EQ(fs_unmount_point("ram"), 0);

    TEST_SECTION("a version 1 disk still works, flat");
    CHECK_EQ(fs_format_version(1), 0);
    CHECK_EQ(fs_version("disk"), 1);
    make("a/b", "flat");                                // '/' is just a character there
    CHECK_EQ(fs_stat("a/b", &st), 0);
    CHECK_EQ(fs_mkdir("dir"), -1);
    CHECK_EQ(errno, ENOTSUP);

    TEST_SECTION("fs_check finds and repairs");
    CHECK_EQ(fs_format(), 0);
    make("big", "0123456789");
    CHECK_EQ(fs_mkdir("d"), 0);
    make("d/inner", "x");
    struct fs_check_report r;
    CHECK_EQ(fs_check("disk", 0, &r), 0);
    CHECK_EQ((int)r.files, 2);
    CHECK_EQ((int)r.directories, 1);
    CHECK_EQ((int)r.used_clusters, 3);
    CHECK_EQ(fs_unmount(), 0);
    // Break it behind the file system's back: a cluster taken in the FAT that nothing reaches, and a size its
    // chain cannot hold.
    unsigned int sb[128], sector[128];
    disk_read(0, sb);
    disk_read(sb[3], sector);
    ((unsigned char*)sector)[50 * 2] = 0xFF;            // cluster 50: "end of a chain", of no chain
    ((unsigned char*)sector)[50 * 2 + 1] = 0xFF;
    disk_write(sb[3], sector);
    disk_read(sb[5], sector);                           // the root: "big" is its first entry
    CHECK_STR((const char*)sector, "big");
    sector[52 / 4] = 5000;                              // its size, far past its one cluster
    disk_write(sb[5], sector);
    CHECK_EQ(fs_mount(), 0);
    CHECK_EQ(fs_check("disk", 0, &r), 2);
    CHECK_EQ((int)r.lost_clusters, 1);
    CHECK_EQ((int)r.bad_sizes, 1);
    CHECK_EQ(fs_check("disk", 1, &r), 2);               // repaired
    CHECK_EQ(fs_check("disk", 0, &r), 0);               // and sound again
    CHECK_EQ(fs_stat("big", &st), 0);
    CHECK_EQ((int)st.size, 512);                        // what its chain can hold
    int fd = fs_open("big", FS_O_RDONLY);
    CHECK_EQ(fs_check("disk", 1, &r), -1);              // not with a file open
    CHECK_EQ(errno, EBUSY);
    fs_close(fd);
    return test_summary();
}
