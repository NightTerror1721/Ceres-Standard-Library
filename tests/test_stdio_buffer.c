// The FILE layer's buffers (M7): disk and host files move blocks, setvbuf chooses how a stream holds bytes back,
// and freopen sends stdout to a file and back. Runs with --host-dir build/host (tests/expected/test_stdio_buffer.run).
#include "ceres/test.h"
#include "ceres/fs.h"
#include "ceres/hostfs.h"
#include "ceres/timer.h"
#include "stdio.h"
#include "string.h"
#include "errno.h"

static char block[3000];

// Instructions to write 2000 bytes one fputc at a time to a disk file with the given buffering.
static int cost_of_putc(const char* name, int mode)
{
    FILE* f = fopen(name, "w");
    setvbuf(f, 0, mode, 0);
    uint64_t start = timer_ticks64();
    for (int i = 0; i < 2000; i++)
        fputc('a' + i % 26, f);
    int cost = (int)(timer_ticks64() - start);
    fclose(f);
    return cost;
}

int main(void)
{
    TEST_SECTION("a byte at a time goes through the buffer");
    CHECK_EQ(fs_format(), 0);
    int unbuffered = cost_of_putc("slow", _IONBF);
    int buffered = cost_of_putc("fast", _IOFBF);
    CHECK(buffered * 3 < unbuffered);                   // the medium sees a few blocks instead of 2000 calls
    FILE* f = fopen("fast", "r");
    int same = 1;
    for (int i = 0; i < 2000; i++)
        same = same && fgetc(f) == 'a' + i % 26;
    CHECK(same);
    CHECK_EQ(fgetc(f), EOF);
    CHECK(feof(f) != 0);
    fclose(f);

    TEST_SECTION("positions count what the buffer holds");
    f = fopen("pos", "w+");
    CHECK_EQ(fputs("0123456789", f), 1);
    CHECK_EQ(ftell(f), 10);                             // still in the buffer, and counted
    CHECK_EQ(fseek(f, 2, SEEK_SET), 0);                 // the seek writes it out first
    CHECK_EQ(fgetc(f), '2');
    CHECK_EQ(ftell(f), 3);                              // the rest was read ahead, and is not counted
    CHECK_EQ(fseek(f, 1, SEEK_CUR), 0);
    CHECK_EQ(fgetc(f), '4');
    CHECK_EQ(fputc('X', f), 'X');                       // reading, then writing: where the reading stopped
    CHECK_EQ(fseek(f, 0, SEEK_SET), 0);
    char line[16] = { 0 };
    CHECK(fgets(line, sizeof line, f) == line);
    CHECK_STR(line, "01234X6789");
    fclose(f);

    TEST_SECTION("large blocks go straight through");
    for (int i = 0; i < (int)sizeof block; i++)
        block[i] = (char)(i * 7);
    f = fopen("big", "w");
    CHECK_EQ(fputc('<', f), '<');
    CHECK_EQ((int)fwrite(block, 1, sizeof block, f), (int)sizeof block);
    CHECK_EQ(fputc('>', f), '>');
    CHECK_EQ(fclose(f), 0);
    f = fopen("big", "r");
    CHECK_EQ(fgetc(f), '<');
    static char back[3000];
    CHECK_EQ((int)fread(back, 1, sizeof back, f), (int)sizeof back);
    CHECK(memcmp(back, block, sizeof block) == 0);
    CHECK_EQ(fgetc(f), '>');
    CHECK_EQ((int)fread(back, 1, 10, f), 0);
    fclose(f);

    TEST_SECTION("a buffer of the program's own, and a line buffer");
    static char mine[64];
    f = fopen("mine", "w");
    CHECK_EQ(setvbuf(f, mine, _IOFBF, sizeof mine), 0);
    fputs("held", f);
    CHECK(memcmp(mine, "held", 4) == 0);                // it is in the program's buffer
    CHECK_EQ(fflush(f), 0);
    fclose(f);
    CHECK_EQ(setvbuf(stdout, 0, 7, 0), -1);              // not a mode
    CHECK_EQ(errno, EINVAL);
    CHECK_EQ(setvbuf(stdout, 0, _IOLBF, 0), 0);
    printf("a line ");                                  // held until the newline, and in order with putchar
    putchar('b');
    fputs(" and more\n", stdout);
    CHECK_EQ(setvbuf(stdout, 0, _IONBF, 0), 0);

    TEST_SECTION("host files");
    FILE* h = fopen("host:notes.txt", "w");
    CHECK(h != 0);
    fprintf(h, "%d apples\n", 3);
    fputs("2 pears\n", h);
    CHECK_EQ(fclose(h), 0);
    CHECK_EQ(host_stat("notes.txt"), 17);
    h = fopen("host:notes.txt", "r");
    int apples = 0;
    CHECK_EQ(fscanf(h, "%d", &apples), 1);
    CHECK_EQ(apples, 3);
    CHECK(fgets(line, sizeof line, h) == line);
    CHECK_STR(line, " apples\n");
    fclose(h);
    CHECK(fopen("host:missing.txt", "r") == 0);
    CHECK_EQ(errno, ENOENT);
    CHECK_EQ(rename("host:notes.txt", "host:kept.txt"), 0);
    CHECK_EQ(rename("host:kept.txt", "kept"), -1);      // not between the host and the disk
    CHECK_EQ(errno, EXDEV);

    TEST_SECTION("stdout to a file and back");
    CHECK(freopen("host:out.txt", "w", stdout) == stdout);
    printf("to the file %d\n", 1);
    puts("puts too");
    CHECK(freopen("term:", "w", stdout) == stdout);     // back on the terminal
    char text[64] = { 0 };
    h = fopen("host:out.txt", "r");
    CHECK_EQ((int)fread(text, 1, sizeof text - 1, h), 23);
    fclose(h);
    CHECK_STR(text, "to the file 1\nputs too\n");
    CHECK_EQ(remove("host:out.txt"), 0);
    CHECK_EQ(remove("host:kept.txt"), 0);
    printf("printing again\n");
    return test_summary();
}
