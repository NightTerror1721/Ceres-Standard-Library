// Logging, the hex dump, the stack watermark and the ANSI sequences. The ANSI test prints each
// sequence on its own line, so the expected file shows exactly which bytes were sent.
#include "ceres/test.h"
#include "ceres/debug.h"
#include "ceres/ansi.h"

static void logging(void)
{
    TEST_SECTION("log levels");
    CHECK_EQ(log_level(), LOG_INFO);                    // the default
    LOGE("error %d", 1);
    LOGW("warning %s", "two");
    LOGI("info %c%c", 'x', 'y');
    LOGD("debug is hidden by default");
    log_set_level(LOG_DEBUG);
    CHECK_EQ(log_level(), LOG_DEBUG);
    LOGD("debug now %u", 3u);
    log_set_level(LOG_ERROR);
    LOGW("hidden");
    LOGI("hidden");
    LOGE("only errors");
    log_set_level(-1);                                  // below the lowest: nothing at all
    LOGE("hidden too");
    log_set_level(LOG_INFO);
    LOGI("back to info: %5.2f", 3.14159f);
}

static void hexdump(void)
{
    TEST_SECTION("hexdump");
    unsigned char data[37];
    for (int i = 0; i < 37; i++)
        data[i] = (unsigned char)(i < 16 ? 'A' + i : i * 7);
    data[3] = 0;
    data[20] = 127;
    dbg_hexdump_base(data, 37, 0x00001000u);
    dbg_hexdump_base(data, 0, 0x2000u);                 // nothing to show: nothing printed
    dbg_hexdump_base("Hi", 2, 0xFFFFFFF0u);             // a short line is padded so the text stays in its column
}

static unsigned int deep(int n)
{
    volatile char pad[2000];
    for (int i = 0; i < 2000; i++) pad[i] = (char)(i + n);
    unsigned int s = 0;
    for (int i = 0; i < 2000; i += 50) s += (unsigned char)pad[i];
    return s;
}

static void stack(void)
{
    TEST_SECTION("stack watermark");
    CHECK_EQ((int)dbg_stack_used(), 0);                 // nothing painted yet
    dbg_stack_paint(8192);
    unsigned int quiet = dbg_stack_used();
    CHECK_EQ((int)quiet, 0);                            // nothing has run below the paint point yet
    (void)deep(1);
    unsigned int after = dbg_stack_used();
    CHECK(after >= 1500);                               // the 2000-byte array, less the frame of the paint call itself
    CHECK(after < 2600);
    (void)deep(2);
    CHECK(dbg_stack_used() >= after);                   // a high-water mark: it never goes down
    dbg_stack_paint(8192);                              // painting again starts over
    CHECK_EQ((int)dbg_stack_used(), 0);
    dbg_stack_paint(0);                                 // an empty window
    CHECK_EQ((int)dbg_stack_used(), 0);

    TEST_SECTION("spans count cycles");
    struct dbg_span s;
    dbg_span_begin(&s, "loop");
    volatile int sink = 0;
    for (int i = 0; i < 1000; i++) sink += i;
    unsigned int used = dbg_span_elapsed(&s);
    CHECK(used > 3000);
    CHECK(used < 200000);                               // CPU cycles: about 92 000 at -O0, 16 000 at -O2
    CHECK(dbg_span_elapsed(&s) >= used);
    dbg_span_begin(&s, "nothing");
    CHECK(dbg_span_elapsed(&s) < 200);
}

static void ansi(void)
{
    TEST_SECTION("ansi");
    ansi_goto(5, 3);         putstr("|goto\n");
    ansi_goto(0, -2);        putstr("|goto clamps to 1;1\n");
    ansi_fg(ANSI_RED);       putstr("|fg red\n");
    ansi_bg(ANSI_BLUE);      putstr("|bg blue\n");
    ansi_fg(ANSI_WHITE);     putstr("|fg white\n");
    ansi_fg(9);              putstr("|fg wraps to 1\n");
    ansi_fg256(200);         putstr("|fg 200\n");
    ansi_bg256(16);          putstr("|bg 16\n");
    ansi_fg256(300);         putstr("|fg 300 wraps to 44\n");
    ansi_up(3);              putstr("|up 3\n");
    ansi_down(12);           putstr("|down 12\n");
    ansi_right(1);           putstr("|right 1\n");
    ansi_left(80);           putstr("|left 80\n");
    ansi_up(0);              putstr("|up 0 sends nothing\n");
    ansi_left(-4);           putstr("|left -4 sends nothing\n");
    ansi_clear_line();       putstr("|clear line\n");
    ansi_save_cursor();      putstr("|save\n");
    ansi_restore_cursor();   putstr("|restore\n");
    putstr(ANSI_CLEAR);      putstr("|clear\n");
    putstr(ANSI_HOME);       putstr("|home\n");
    putstr(ANSI_HIDE_CUR);   putstr("|hide\n");
    putstr(ANSI_SHOW_CUR);   putstr("|show\n");
    putstr(ANSI_BOLD);       putstr("|bold\n");
    putstr(ANSI_RESET);      putstr("|reset\n");
}

int main(void)
{
    logging();
    hexdump();
    stack();
    ansi();
    return test_summary();
}
