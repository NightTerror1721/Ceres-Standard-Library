// A disk file is buffered by CeresFS. C closes every open stream when a program ends, so one that is never
// closed or flushed still reaches the disk when main returns; and fflush(NULL) writes every open file out at
// once. The disk is read raw (sector by sector) to see what has actually been written, and the last check runs
// from an atexit handler registered BEFORE the first fopen: handlers run last-registered first, so it runs
// after the one fopen registered.
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "ceres/fs.h"
#include "ceres/disk.h"

// Is `needle` in any sector of the disk?
static int on_disk(const char* needle)
{
    static char sector[DISK_SECTOR_SIZE];
    size_t n = strlen(needle);
    for (unsigned int s = 0; s < disk_sectors(); s++)
    {
        disk_read(s, sector);
        for (size_t i = 0; i + n <= DISK_SECTOR_SIZE; i++)
            if (memcmp(sector + i, needle, n) == 0)
                return 1;
    }
    return 0;
}

static void after_the_program_ends(void)
{
    printf("at exit, after the sync: closed-by-nobody file is on the disk: %s\n", on_disk("written and never closed") ? "yes" : "no");
}

int main(void)
{
    atexit(after_the_program_ends);
    fs_format();

    FILE* flushed = fopen("flushed.txt", "w");
    fputs("written and flushed with fflush(NULL)", flushed);
    printf("before fflush(NULL): flushed file on the disk: %s\n", on_disk("flushed with fflush(NULL)") ? "yes" : "no");
    fflush(0);
    printf("after fflush(NULL):  flushed file on the disk: %s\n", on_disk("flushed with fflush(NULL)") ? "yes" : "no");

    FILE* open_at_exit = fopen("open.txt", "w");
    fputs("written and never closed", open_at_exit);
    puts("main returns with two files still open");
    return 0;
}
