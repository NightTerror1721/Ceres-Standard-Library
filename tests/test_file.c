// Streams: memory streams (fmemopen) in every mode, the terminal streams, ungetc, getline, and the
// the disk functions on a disk with no file system (ENODEV; test_disk_fs covers the real thing). stdin is tests/expected/test_file.stdin
// (under 64 bytes: the host pushes it into a 64-byte ring as fast as it can).
#include "ceres/test.h"
#include "errno.h"
#include "stdlib.h"
#include "string.h"

int main(void)
{
    char mem[64];

    TEST_SECTION("writing to memory");
    memset(mem, 'Z', sizeof(mem));
    FILE* w = fmemopen(mem, sizeof(mem), "w");
    CHECK(w != 0);
    CHECK_EQ(mem[0], 0);                                          // "w" empties the buffer
    CHECK_EQ(fprintf(w, "x=%d\n", 42), 5);
    CHECK(fputs("abc", w) >= 0);
    CHECK_EQ(fputc('!', w), '!');
    CHECK_EQ(putc('?', w), '?');
    CHECK_EQ((int)fwrite("12", 1, 2, w), 2);
    CHECK_EQ((int)fwrite("ab", 2, 1, w), 1);
    CHECK_EQ(ftell(w), 14);
    CHECK_STR(mem, "x=42\nabc!?12ab");                            // always NUL-terminated while there is room
    CHECK_EQ(fflush(w), 0);
    CHECK_EQ(fclose(w), 0);
    CHECK_STR(mem, "x=42\nabc!?12ab");

    TEST_SECTION("reading from memory");
    char text[] = "first line\nsecond\nthird";
    FILE* r = fmemopen(text, strlen(text), "r");
    char line[32];
    CHECK(fgets(line, sizeof(line), r) == line);
    CHECK_STR(line, "first line\n");
    CHECK_EQ(fgetc(r), 's');
    CHECK_EQ(ungetc('S', r), 'S');                                // a different character than was read
    CHECK_EQ(fgetc(r), 'S');
    CHECK(fgets(line, sizeof(line), r) != 0);
    CHECK_STR(line, "econd\n");
    CHECK_EQ(feof(r), 0);
    CHECK(fgets(line, 3, r) == line);                             // at most n-1 characters
    CHECK_STR(line, "th");
    CHECK(fgets(line, sizeof(line), r) == line);
    CHECK_STR(line, "ird");                                       // the last line has no newline
    CHECK_EQ(feof(r), 1);                                         // a line with no newline is only over when the read meets the end
    CHECK(fgets(line, sizeof(line), r) == 0);
    CHECK_EQ(feof(r), 1);
    CHECK_EQ(fgetc(r), EOF);
    CHECK_EQ(ferror(r), 0);
    CHECK_EQ(ungetc('x', r), 'x');                                // pushing a character back clears the end
    CHECK_EQ(feof(r), 0);
    CHECK_EQ(fgetc(r), 'x');
    CHECK_EQ(ungetc(EOF, r), EOF);                                // EOF cannot be pushed back
    clearerr(r);
    CHECK_EQ(feof(r), 0);

    TEST_SECTION("seek and tell");
    CHECK_EQ(ftell(r), 23);                                       // the whole text (23 bytes) has been read
    CHECK_EQ(fseek(r, 0, SEEK_SET), 0);
    CHECK_EQ(ftell(r), 0);
    CHECK_EQ(fgetc(r), 'f');
    CHECK_EQ(ftell(r), 1);
    CHECK_EQ(fseek(r, 5, SEEK_CUR), 0);
    CHECK_EQ(fgetc(r), 'l');                                      // position 6 of "first line" is the 'l'
    CHECK_EQ(fseek(r, -5, SEEK_END), 0);
    CHECK_EQ(fgetc(r), 't');                                      // "third" starts 5 from the end
    CHECK_EQ(fseek(r, -100, SEEK_SET), -1);                       // before the start
    CHECK_EQ(errno, EINVAL);
    CHECK_EQ(fseek(r, 1000, SEEK_SET), -1);                       // past the buffer
    CHECK_EQ(fseek(r, 0, 99), -1);                                // no such whence
    fpos_t mark;
    fseek(r, 6, SEEK_SET);
    CHECK_EQ(fgetpos(r, &mark), 0);
    fgetc(r);
    fgetc(r);
    CHECK_EQ(fsetpos(r, &mark), 0);
    CHECK_EQ(ftell(r), 6);
    rewind(r);
    CHECK_EQ(ftell(r), 0);
    char block[8];
    CHECK_EQ((int)fread(block, 1, 5, r), 5);
    CHECK(memcmp(block, "first", 5) == 0);
    CHECK_EQ((int)fread(block, 4, 1, r), 1);                      // one element of 4 bytes: " lin"
    CHECK(memcmp(block, " lin", 4) == 0);
    fseek(r, 0, SEEK_END);
    CHECK_EQ((int)fread(block, 1, 8, r), 0);                      // at the end: nothing
    CHECK_EQ(feof(r), 1);
    fseek(r, -3, SEEK_END);
    CHECK_EQ((int)fread(block, 1, 8, r), 3);                      // a short read reports what it got
    CHECK(memcmp(block, "ird", 3) == 0);
    rewind(r);
    errno = 0;
    CHECK_EQ((int)fread(block, 0x10000u, 0x10001u, r), 0);        // 2^32 + 2^16 bytes: the product wraps
    CHECK_EQ(errno, EOVERFLOW);
    CHECK(ferror(r) != 0);
    clearerr(r);
    CHECK_EQ(fgetc(r), 'f');                                      // and nothing was read
    CHECK_EQ(fclose(r), 0);

    TEST_SECTION("fscanf and getline on memory");
    char numbers[] = "10 20 abc\nsecond line\nlast";
    FILE* n = fmemopen(numbers, strlen(numbers), "r");
    int p = 0, q = 0;
    char word[16];
    CHECK_EQ(fscanf(n, "%d %d %s", &p, &q, word), 3);
    CHECK(p == 10 && q == 20);
    CHECK_STR(word, "abc");
    char* got = 0;
    size_t cap = 0;
    CHECK_EQ(getline(&got, &cap, n), 1);                          // the rest of the first line: just "\n"
    CHECK_EQ(getline(&got, &cap, n), 12);
    CHECK_STR(got, "second line\n");
    CHECK(cap >= 13);
    CHECK_EQ(getline(&got, &cap, n), 4);                          // no newline at the end
    CHECK_STR(got, "last");
    CHECK_EQ(getline(&got, &cap, n), -1);                         // then EOF
    CHECK_EQ(feof(n), 1);
    free(got);
    fclose(n);
    char longtext[300];
    memset(longtext, 'q', 250);
    longtext[250] = '\n';
    longtext[251] = 0;
    FILE* lf = fmemopen(longtext, 251, "r");
    got = 0;
    cap = 0;
    CHECK_EQ(getline(&got, &cap, lf), 251);                       // grows past its first buffer
    CHECK(got[0] == 'q' && got[249] == 'q' && got[250] == '\n' && got[251] == 0);
    free(got);
    fclose(lf);

    TEST_SECTION("append and update modes");
    char buf[32] = "abc";
    FILE* ap = fmemopen(buf, sizeof(buf), "a");
    fputs("def", ap);
    fclose(ap);
    CHECK_STR(buf, "abcdef");                                     // "a" writes after the text already there
    FILE* rw = fmemopen(buf, sizeof(buf), "w+");
    fputs("hello", rw);
    rewind(rw);
    CHECK_EQ(fgetc(rw), 'h');                                     // "w+" reads back what it wrote
    CHECK_EQ(fgetc(rw), 'e');
    fseek(rw, 1, SEEK_SET);
    fputc('E', rw);                                               // and overwrites in place
    rewind(rw);
    fgets(line, sizeof(line), rw);
    CHECK_STR(line, "hEllo");
    fclose(rw);
    char fixed[9] = "12345678";
    FILE* up = fmemopen(fixed, 8, "r+");
    CHECK_EQ(fgetc(up), '1');
    fputc('X', up);                                               // "r+" keeps the text and overwrites at the position
    fclose(up);
    CHECK(memcmp(fixed, "1X345678", 8) == 0);

    TEST_SECTION("what a stream refuses");
    char tiny[4];
    FILE* full = fmemopen(tiny, sizeof(tiny), "w");
    CHECK_EQ(fputs("abcdefg", full), EOF);                        // it never grows: the write stops when the buffer is full
    CHECK_EQ(ferror(full), 1);
    CHECK(memcmp(tiny, "abcd", 4) == 0);
    clearerr(full);
    CHECK_EQ(ferror(full), 0);
    CHECK_EQ((int)fwrite("xyz", 1, 3, full), 0);                  // full: nothing more fits
    fclose(full);
    char ro[4] = "ro";
    FILE* readonly = fmemopen(ro, sizeof(ro), "r");
    CHECK_EQ(fputc('x', readonly), EOF);                          // a read-only stream cannot be written
    CHECK_EQ((int)fwrite("x", 1, 1, readonly), 0);
    fclose(readonly);
    char wo[4];
    FILE* writeonly = fmemopen(wo, sizeof(wo), "w");
    CHECK_EQ(fgetc(writeonly), EOF);                              // nor a write-only stream read
    CHECK(fgets(line, sizeof(line), writeonly) == 0);
    fclose(writeonly);
    CHECK(fmemopen(0, 8, "r") == 0);
    CHECK_EQ(errno, EINVAL);
    CHECK(fmemopen(mem, 0, "r") == 0);
    CHECK(fmemopen(mem, 8, "x") == 0);
    CHECK(fmemopen(mem, 8, "rw") == 0);
    CHECK_EQ(fclose(0), EOF);
    CHECK_EQ(fgetc(0), EOF);
    CHECK_EQ(fputc('x', 0), EOF);

    TEST_SECTION("no file system on the disk");
    errno = 0;
    CHECK(fopen("data.txt", "r") == 0);
    CHECK_EQ(errno, ENODEV);                                      // nothing formatted or mounted
    errno = 0;
    CHECK(freopen("data.txt", "r", stdin) == 0);
    CHECK_EQ(errno, ENODEV);                                      // stdin can be reopened: the disk is what fails
    CHECK(tmpfile() == 0);
    errno = 0;
    CHECK_EQ(remove("data.txt"), -1);
    CHECK_EQ(errno, ENODEV);
    CHECK_EQ(rename("a", "b"), -1);
    CHECK_EQ(setvbuf(stdout, 0, _IOFBF, 100), 0);                 // accepted, and nothing changes
    setbuf(stdout, 0);

    TEST_SECTION("stdout and stderr");
    CHECK_EQ(fprintf(stdout, "to stdout %d\n", 1), 12);
    CHECK_EQ(fprintf(stderr, "to stderr %d\n", 2), 12);
    CHECK(fputs("puts to stdout\n", stdout) >= 0);
    CHECK(fputs("puts to stderr\n", stderr) >= 0);
    CHECK_EQ(fputc('a', stdout), 'a');
    CHECK_EQ(putc('b', stderr), 'b');
    putchar('c');
    printf("d");
    fputc('\n', stdout);                                          // "abcd": every route keeps its order
    CHECK_EQ((int)fwrite("fwrite\n", 1, 7, stdout), 7);
    CHECK_EQ((int)fwrite("ab\n", 3, 1, stdout), 1);          // one element of 3 bytes
    fputc('\n', stdout);
    CHECK_EQ(fflush(stdout), 0);
    CHECK_EQ(fflush(0), 0);
    CHECK_EQ(fputc('x', stdin), EOF);                             // stdin is read-only
    CHECK_EQ(fgetc(stdout), EOF);                                 // and stdout write-only
    errno = ENOENT;
    perror("open");
    errno = 0;
    perror(0);
    perror("");
    CHECK_EQ(fseek(stdout, 0, SEEK_SET), -1);                     // the terminal cannot be sought
    CHECK_EQ(errno, ESPIPE);
    CHECK_EQ(ftell(stdout), -1);

    TEST_SECTION("stdin");
    int num = 0;
    float real = 0.0f;
    CHECK_EQ(scanf("%d %s %f", &num, word, &real), 3);           // "12 abc 3.5"
    CHECK_EQ(num, 12);
    CHECK_STR(word, "abc");
    CHECK(real == 3.5f);
    CHECK_EQ(getchar(), '\n');                                    // scanf left the newline where it found it
    CHECK(fgets(line, sizeof(line), stdin) == line);
    CHECK_STR(line, "second line\n");
    CHECK_EQ(getc(stdin), 'x');
    CHECK_EQ(ungetc('x', stdin), 'x');
    CHECK_EQ(ungetc('y', stdin), EOF);                            // one character of pushback
    CHECK_EQ(getchar(), 'x');                                     // and getchar sees it: the same stream
    CHECK_EQ(getchar_nb(), '\n');
    CHECK_EQ(getchar_nb(), -1);                                   // nothing else is waiting
    ungetc('!', stdin);
    CHECK_EQ(getchar_nb(), '!');
    CHECK_EQ(feof(stdin), 0);                                     // the terminal never says "end"
    CHECK_EQ(ferror(stdin), 0);
    CHECK_EQ(fseek(stdin, 0, SEEK_SET), -1);
    CHECK_EQ(ftell(stdin), -1);
    return test_summary();
}
