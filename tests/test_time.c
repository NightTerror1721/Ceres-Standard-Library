// <time.h> and ceres/timer.h. The calendar is checked against known dates; time() is only sanity-checked,
// because the wall clock is the one value in the machine that is not deterministic.
#include "ceres/test.h"
#include "time.h"
#include "ceres/timer.h"
#include "string.h"
#include "errno.h"

static void expect_date(time_t t, int year, int mon, int mday, int hour, int min, int sec, int wday, int yday)
{
    struct tm tm;
    gmtime_r(&t, &tm);
    __t_total++;
    if (tm.tm_year + 1900 != year || tm.tm_mon + 1 != mon || tm.tm_mday != mday || tm.tm_hour != hour ||
        tm.tm_min != min || tm.tm_sec != sec || tm.tm_wday != wday || tm.tm_yday != yday)
    {
        __t_failed++;
        printf("FAIL gmtime(%lld): got %d-%d-%d %d:%d:%d wday %d yday %d\n", t, tm.tm_year + 1900, tm.tm_mon + 1,
               tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_wday, tm.tm_yday);
    }
    __t_total++;
    if (mktime(&tm) != t)                            // and back again
    {
        __t_failed++;
        printf("FAIL mktime does not return %lld\n", t);
    }
}

static struct tm make(int year, int mon, int mday, int hour, int min, int sec)
{
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = year - 1900;
    tm.tm_mon = mon - 1;
    tm.tm_mday = mday;
    tm.tm_hour = hour;
    tm.tm_min = min;
    tm.tm_sec = sec;
    return tm;
}

static int fired_a, fired_b, fired_c;
static void on_a(void* ctx) { fired_a++; }
static void on_b(void* ctx) { fired_b += *(int*)ctx; }
static void on_c(void* ctx) { fired_c++; }
static int cancel_me;
static void cancel_self(void* ctx) { fired_c++; timer_cancel(cancel_me); }
static void schedule_more(void* ctx) { timer_after(0, on_a, 0); }

int main(void)
{
    TEST_SECTION("gmtime and mktime round trip");
    //          time        Y     M   D   h   m   s   wday yday
    expect_date(0u,          1970, 1,  1,  0,  0,  0,  4,   0);
    expect_date(86399u,      1970, 1,  1,  23, 59, 59, 4,   0);
    expect_date(86400u,      1970, 1,  2,  0,  0,  0,  5,   1);
    expect_date(951782400u,  2000, 2,  29, 0,  0,  0,  2,   59);      // a leap day (2000 is a leap year)
    expect_date(951868800u,  2000, 3,  1,  0,  0,  0,  3,   60);
    expect_date(1234567890u, 2009, 2,  13, 23, 31, 30, 5,   43);
    expect_date(1709164800u, 2024, 2,  29, 0,  0,  0,  4,   59);
    expect_date(1735689599u, 2024, 12, 31, 23, 59, 59, 2,   365);
    expect_date(2147483647u, 2038, 1,  19, 3,  14, 7,  2,   18);      // where a 32-bit signed time_t stopped
    expect_date(4107456000u, 2100, 2,  28, 0,  0,  0,  0,   58);
    expect_date(4107542400u, 2100, 3,  1,  0,  0,  0,  1,   59);      // 2100 is NOT a leap year: no Feb 29
    expect_date(4294967295u, 2106, 2,  7,  6,  28, 15, 0,   37);      // the last second the clock register counts
    expect_date(4294967296LL, 2106, 2, 7,  6,  28, 16, 0,   37);      // and time_t goes on (M12)
    expect_date(253402300799LL, 9999, 12, 31, 23, 59, 59, 5, 364);
    expect_date(-1LL,        1969, 12, 31, 23, 59, 59, 3,   364);     // before 1970 is a negative time
    expect_date(-2208988800LL, 1900, 1, 1, 0,  0,  0,  1,   0);       // 1900 was not a leap year
    expect_date(-62135596800LL, 1, 1,  1,  0,  0,  0,  1,   0);       // the first day of year 1, proleptic Gregorian

    TEST_SECTION("mktime normalizes");
    struct tm a = make(2024, 12, 32, 0, 0, 0);                       // December 32nd
    CHECK(mktime(&a) != (time_t)-1);
    CHECK(a.tm_year + 1900 == 2025 && a.tm_mon == 0 && a.tm_mday == 1);
    CHECK_EQ(a.tm_wday, 3);                                          // 2025-01-01 was a Wednesday
    struct tm b = make(2024, 13, 1, 0, 0, 0);                        // month 13 = January of the next year
    mktime(&b);
    CHECK(b.tm_year + 1900 == 2025 && b.tm_mon == 0 && b.tm_mday == 1);
    struct tm c = make(2024, 3, 0, 0, 0, 0);                         // day 0 = the last of the month before
    mktime(&c);
    CHECK(c.tm_mon == 1 && c.tm_mday == 29);                         // February 29th 2024
    struct tm d = make(2023, 3, 0, 0, 0, 0);
    mktime(&d);
    CHECK(d.tm_mon == 1 && d.tm_mday == 28);
    struct tm e = make(2000, 1, 1, 0, 0, -1);                        // second -1
    mktime(&e);
    CHECK(e.tm_year + 1900 == 1999 && e.tm_mon == 11 && e.tm_mday == 31 && e.tm_hour == 23 && e.tm_min == 59 && e.tm_sec == 59);
    struct tm f = make(2000, 1, 1, 25, 61, 61);                      // overflowing hours, minutes and seconds
    mktime(&f);
    CHECK(f.tm_mday == 2 && f.tm_hour == 2 && f.tm_min == 2 && f.tm_sec == 1);
    struct tm g = make(2000, -1, 15, 0, 0, 0);                       // month -1 = November of the year before
    mktime(&g);
    CHECK(g.tm_year + 1900 == 1999 && g.tm_mon == 10);
    struct tm h = make(1970, 1, 1, 0, 0, 0);
    CHECK_EQ((int)mktime(&h), 0);
    struct tm before = make(1969, 12, 31, 23, 59, 58);
    CHECK(mktime(&before) == -2);                                    // before the epoch: a negative time
    struct tm after = make(2106, 2, 7, 6, 28, 16);
    CHECK(mktime(&after) == 4294967296LL);                           // 2106 is not the end any more
    struct tm last = make(2106, 2, 7, 6, 28, 15);
    CHECK(mktime(&last) == 4294967295LL);
    struct tm distant = make(2200, 1, 1, 0, 0, 0);
    CHECK(mktime(&distant) == 7258118400LL);
    struct tm too_far = make(6000000, 1, 1, 0, 0, 0);
    errno = 0;
    CHECK(mktime(&too_far) == (time_t)-1);                           // past what the calendar keeps
    CHECK_EQ(errno, EOVERFLOW);
    time_t edge = 170000000000000LL;                                 // about 5.39 million years on: gmtime's bound is mktime's
    struct tm at_edge;
    CHECK(gmtime_r(&edge, &at_edge) != 0);
    CHECK(mktime(&at_edge) == edge);

    TEST_SECTION("gmtime, localtime and ctime");
    time_t when = 1234567890u;
    struct tm* shared = gmtime(&when);
    CHECK(shared->tm_year == 109 && shared->tm_mon == 1 && shared->tm_mday == 13);
    CHECK(localtime(&when) == shared);                               // one static buffer, and no time zones
    CHECK_EQ(shared->tm_isdst, 0);
    CHECK_STR(asctime(gmtime(&when)), "Fri Feb 13 23:31:30 2009\n");
    CHECK_STR(ctime(&when), "Fri Feb 13 23:31:30 2009\n");
    time_t zero = 0;
    CHECK_STR(ctime(&zero), "Thu Jan  1 00:00:00 1970\n");           // the day is padded with a space
    time_t mid = 951782400u;
    CHECK_STR(ctime(&mid), "Tue Feb 29 00:00:00 2000\n");
    time_t huge = 9000000000000000000LL;                             // no int holds its year
    errno = 0;
    CHECK(gmtime(&huge) == 0);
    CHECK_EQ(errno, EOVERFLOW);
    CHECK(ctime(&huge) == 0);

    TEST_SECTION("strftime");
    struct tm tm;
    time_t sample = 1234567890u;                                     // Fri 2009-02-13 23:31:30, day 44 of the year
    gmtime_r(&sample, &tm);
    char buf[128];
    CHECK_EQ((int)strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm), 19);
    CHECK_STR(buf, "2009-02-13 23:31:30");
    strftime(buf, sizeof(buf), "%a %A %b %B %h", &tm);
    CHECK_STR(buf, "Fri Friday Feb February Feb");
    strftime(buf, sizeof(buf), "%y %C %j %u %w %e|%d", &tm);
    CHECK_STR(buf, "09 20 044 5 5 13|13");
    strftime(buf, sizeof(buf), "%I:%M %p", &tm);
    CHECK_STR(buf, "11:31 PM");
    strftime(buf, sizeof(buf), "%F %T %D %R", &tm);
    CHECK_STR(buf, "2009-02-13 23:31:30 02/13/09 23:31");
    strftime(buf, sizeof(buf), "%c", &tm);
    CHECK_STR(buf, "Fri Feb 13 23:31:30 2009");
    strftime(buf, sizeof(buf), "%x %X", &tm);
    CHECK_STR(buf, "02/13/09 23:31:30");
    strftime(buf, sizeof(buf), "%Z %z %s", &tm);
    CHECK_STR(buf, "UTC +0000 1234567890");
    strftime(buf, sizeof(buf), "100%%%n%t.", &tm);
    CHECK_STR(buf, "100%\n\t.");
    strftime(buf, sizeof(buf), "%q %", &tm);
    CHECK_STR(buf, "%q %");                                          // unknown and dangling: printed back
    time_t noon = 951782400u + 12 * 3600;
    gmtime_r(&noon, &tm);
    strftime(buf, sizeof(buf), "%I %p", &tm);
    CHECK_STR(buf, "12 PM");
    time_t midnight = 951782400u;
    gmtime_r(&midnight, &tm);
    strftime(buf, sizeof(buf), "%I %p %u", &tm);
    CHECK_STR(buf, "12 AM 2");
    time_t sunday = 4294967295u;
    gmtime_r(&sunday, &tm);
    strftime(buf, sizeof(buf), "%u %w", &tm);
    CHECK_STR(buf, "7 0");                                           // Sunday is 7 in %u and 0 in %w
    time_t early = -1;
    gmtime_r(&early, &tm);
    strftime(buf, sizeof(buf), "%s %F %T", &tm);
    CHECK_STR(buf, "-1 1969-12-31 23:59:59");
    gmtime_r(&sample, &tm);
    CHECK_EQ((int)strftime(buf, 5, "%Y-%m", &tm), 0);                // does not fit: nothing, and a NUL
    CHECK_EQ(buf[0], 0);
    CHECK_EQ((int)strftime(buf, 8, "%Y-%m", &tm), 7);                // exactly fits with its NUL
    CHECK_STR(buf, "2009-02");
    CHECK_EQ((int)strftime(buf, 7, "%Y-%m", &tm), 0);                // one short
    CHECK_EQ((int)strftime(buf, 0, "x", &tm), 0);
    CHECK_EQ((int)strftime(buf, sizeof(buf), "", &tm), 0);           // an empty result is 0 and fine
    CHECK_STR(buf, "");

    TEST_SECTION("clocks");
    CHECK(difftime(100u, 40u) == 60.0f);
    CHECK(difftime(40u, 100u) == -60.0f);
    CHECK(difftime(4294967295u, 0u) == 4294967296.0f);               // (float can only be close here)
    time_t now = 0;
    time_t returned = time(&now);
    CHECK(now == returned);
    CHECK(now > 1600000000);                                         // after September 2020: the clock is set
    CHECK(time(0) >= now);
    clock_t c1 = clock();
    timer_wait_ms(3);
    clock_t c2 = clock();
    CHECK(c2 - c1 >= 3000);                                          // microseconds of real time (M12)
    CHECK(c2 - c1 < 1000000);
    CHECK_EQ((int)CLOCKS_PER_SEC, 1000000);
    CHECK(sizeof(time_t) == 8 && sizeof(clock_t) == 8 && (time_t)-1 < 0);
    CHECK_EQ((int)sleep(0), 0);

    TEST_SECTION("timer ticks");
    unsigned int t0 = timer_ticks();
    unsigned int t1 = timer_ticks();
    CHECK(t1 > t0);
    uint64_t w0 = timer_cycles64();
    uint64_t w1 = timer_cycles64();
    CHECK(w1 > w0);                                                  // all 64 bits, read as one moment
    CHECK((unsigned int)w1 - t1 < 100000u);                          // whose low word is timer_ticks()
    CHECK(timer_elapsed(t0) >= t1 - t0);
    CHECK(timer_elapsed(t1 + 1000000u) > 0xFFF00000u);               // a "since" in the future wraps: that is the arithmetic
    unsigned int before_wait = timer_ticks();
    timer_wait(2000);
    CHECK(timer_elapsed(before_wait) >= 2000u);
    unsigned int deadline = timer_ticks() + 1500u;
    timer_wait_until(deadline);
    CHECK((int)(timer_ticks() - deadline) >= 0);
    timer_wait_until(deadline - 100000u);                            // a deadline already past returns at once
    CHECK(timer_clock() > 1600000000u);
    timer_arm(1000, 0);                                              // the hardware timer arms and disarms cleanly
    timer_disarm();
    timer_arm(0, 1);                                                 // 0 becomes 1: it must not silently disarm
    timer_disarm();

    TEST_SECTION("software timers");
    fired_a = fired_b = fired_c = 0;
    int weight = 5;
    // The tick register runs faster than a loop's instruction count suggests, so the periods are generous.
    int id_a = timer_after(600000, on_a, 0);
    int id_b = timer_after(200000, on_b, &weight);
    int id_c = timer_every(100000, on_c, 0);
    CHECK(id_a >= 0 && id_b >= 0 && id_c >= 0);
    CHECK(id_a != id_b && id_b != id_c && id_a != id_c);
    CHECK_EQ(timer_pending(), 3);
    CHECK_EQ(timer_poll(), 0);                                       // nothing is due yet
    CHECK_EQ(fired_a + fired_b + fired_c, 0);
    unsigned int began = timer_ticks();
    while (timer_elapsed(began) < 700000u)
        timer_poll();
    CHECK_EQ(fired_a, 1);                                            // one-shots ran exactly once ...
    CHECK_EQ(fired_b, 5);                                            // ... with their own context
    CHECK(fired_c >= 6 && fired_c <= 8);                             // every 100000 over 700000 ticks: about 7
    CHECK_EQ(timer_pending(), 1);                                    // only the periodic one is left
    timer_cancel(id_c);
    CHECK_EQ(timer_pending(), 0);
    int seen = fired_c;
    timer_wait(300000);
    timer_poll();
    CHECK_EQ(fired_c, seen);                                         // a cancelled task never runs
    timer_cancel(id_c);                                              // cancelling again is harmless
    timer_cancel(-1);
    timer_cancel(99);

    TEST_SECTION("the table");
    int ids[TIMER_MAX_TASKS];
    for (int i = 0; i < TIMER_MAX_TASKS; i++)
    {
        ids[i] = timer_after(1000000, on_a, 0);
        CHECK(ids[i] >= 0);
    }
    CHECK_EQ(timer_after(1, on_a, 0), -1);                           // full
    CHECK_EQ(timer_every(1, on_a, 0), -1);
    CHECK_EQ(timer_after(1, 0, 0), -1);                              // no callback, no task
    timer_cancel(ids[3]);
    int reused = timer_after(1000000, on_a, 0);
    CHECK_EQ(reused, ids[3]);                                        // the freed slot is reused
    for (int i = 0; i < TIMER_MAX_TASKS; i++)
        timer_cancel(i);
    CHECK_EQ(timer_pending(), 0);

    TEST_SECTION("callbacks that touch the table");
    fired_a = fired_c = 0;
    cancel_me = timer_every(1, cancel_self, 0);                      // cancels itself the first time it runs
    timer_wait(50);
    timer_poll();
    CHECK_EQ(fired_c, 1);
    timer_poll();
    CHECK_EQ(fired_c, 1);
    CHECK_EQ(timer_pending(), 0);
    timer_after(1, schedule_more, 0);                                // this one schedules another when it runs
    timer_wait(50);
    CHECK_EQ(timer_poll(), 1);
    CHECK_EQ(timer_pending(), 1);                                    // the new task is waiting, not lost
    timer_wait(50);
    CHECK_EQ(timer_poll(), 1);
    CHECK_EQ(fired_a, 1);
    CHECK_EQ(timer_pending(), 0);
    return test_summary();
}
