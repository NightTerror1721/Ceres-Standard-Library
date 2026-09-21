// Files on the disk: CeresFS through the ordinary stdio calls.
//
// Run with a disk image and the files survive between runs:
//
//     ceres run fsdemo.cres --disk demo.img      (or: tools\example.ps1 fsdemo, which runs it without one)
//
// The first run formats the image; every run after it finds the earlier files and adds to them. Without
// --disk the disk lives only as long as the program, so every run is the first one.
#include "stdio.h"
#include "string.h"
#include "ceres/fs.h"

static int show(const struct fs_dirent* d, void* ctx)
{
    printf("  %-12s %5u bytes\n", d->name, d->size);
    return 0;
}

int main(void)
{
    if (fs_mount() == 0)
        puts("found a CeresFS on the disk");
    else if (fs_format() == 0)
        printf("formatted a new CeresFS: %u bytes of room\n", fs_total_bytes());
    else
    {
        perror("no disk");
        return 1;
    }

    // a counter that lives in a file
    int runs = 0;
    FILE* f = fopen("runs.txt", "r");
    if (f != NULL)
    {
        fscanf(f, "%d", &runs);
        fclose(f);
    }
    runs++;
    f = fopen("runs.txt", "w");
    fprintf(f, "%d\n", runs);
    fclose(f);
    printf("this is run %d\n", runs);

    // a log that only grows
    f = fopen("log.txt", "a");
    fprintf(f, "run %d: %u bytes free\n", runs, fs_free_bytes());
    fclose(f);

    // read it back a line at a time
    puts("log.txt:");
    f = fopen("log.txt", "r");
    char line[64];
    int n = 0;
    while (fgets(line, sizeof line, f) != NULL)
        printf("  %d: %s", ++n, line);
    fclose(f);

    puts("files:");
    fs_list(show, NULL);
    printf("%u of %u bytes free\n", fs_free_bytes(), fs_total_bytes());
    return 0;
}
