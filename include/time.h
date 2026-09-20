#pragma once

#include "stddef.h"

// Calendar time. There are no time zones: localtime() is gmtime(). time_t is an UNSIGNED 32-bit count
// of seconds since 1970-01-01T00:00:00Z, so it runs out on 2106-02-07 (a signed one would stop at 2038)
// and cannot hold a date before 1970.

typedef unsigned int time_t;
typedef unsigned int clock_t;
#define CLOCKS_PER_SEC 1000000     // nominal: clock() counts INSTRUCTIONS, not microseconds (see below)

struct tm
{
    int tm_sec;      // 0..59
    int tm_min;      // 0..59
    int tm_hour;     // 0..23
    int tm_mday;     // 1..31
    int tm_mon;      // 0..11
    int tm_year;     // years since 1900
    int tm_wday;     // 0..6, Sunday is 0
    int tm_yday;     // 0..365
    int tm_isdst;    // always 0
};

// time() reads the wall clock (the timer device's clock register) - the ONE non-deterministic value in
// the machine, so a test must not depend on what it returns.
time_t time(time_t* out);

// clock() is the number of instructions executed so far: the same on every run of the same program.
clock_t clock(void);
float   difftime(time_t end, time_t start);

struct tm* gmtime(const time_t* t);                  // points at ONE static struct, overwritten by the next call
struct tm* localtime(const time_t* t);               // == gmtime
struct tm* gmtime_r(const time_t* t, struct tm* out);
struct tm* localtime_r(const time_t* t, struct tm* out);

// Normalizes the fields (month 12 is January of the next year, day 0 the last of the previous month,
// second -1 the last second of the minute before...), fills tm_wday and tm_yday, and returns the time.
// (time_t)-1 when the date is before 1970 or after 2106.
time_t mktime(struct tm* tm);

char*  asctime(const struct tm* tm);                 // "Thu Jan  1 00:00:00 1970\n", in a static buffer
char*  ctime(const time_t* t);

// %Y %C %y %m %d %e %H %I %M %S %p %a %A %b %B %h %j %u %w %Z %z %s %F %T %D %R %c %x %X %n %t %%
// Returns the length written (without the NUL), or 0 when the result does not fit in `max`.
size_t strftime(char* buf, size_t max, const char* fmt, const struct tm* tm);

// Waits until the wall clock has advanced `seconds`. A spin, not a halt: the machine only wakes from
// a halt on an interrupt that is actually dispatched, and this must not need a handler. Returns 0.
unsigned int sleep(unsigned int seconds);
