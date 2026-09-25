# `<time.h>`

Calendar time. There are no time zones: localtime() is gmtime(). time_t is a SIGNED 64-bit count of seconds since 1970-01-01T00:00:00Z: a date before 1970 is negative, and the calendar functions take any date within about five million years of it. (The machine's clock register counts in 32 unsigned bits, so time() itself is right until 2106.)

```c
typedef long long time_t;
typedef long long clock_t;
#define CLOCKS_PER_SEC 1000000     // clock() counts microseconds of real time (see below)

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
_Static_assert(sizeof(struct tm) == 36, "struct tm is nine words");

// time() reads the wall clock (the timer device's clock register) - the ONE non-deterministic value in
// the machine, so a test must not depend on what it returns.
time_t time(time_t* out);

// clock() is the real time since the machine started, in microseconds - CLOCKS_PER_SEC a second, as C says.
// The machine runs one program, so its time is the program's. (For the instructions executed, the count that
// is the same on every run, use timer_ticks64() in ceres/timer.h.)
clock_t clock(void);
float   difftime(time_t end, time_t start);

// gmtime and localtime return NULL (errno EOVERFLOW) for a time whose year an int cannot hold.
struct tm* gmtime(const time_t* t);                  // points at ONE static struct, overwritten by the next call
struct tm* localtime(const time_t* t);               // == gmtime
struct tm* gmtime_r(const time_t* t, struct tm* out);
struct tm* localtime_r(const time_t* t, struct tm* out);

// Normalizes the fields (month 12 is January of the next year, day 0 the last of the previous month,
// second -1 the last second of the minute before...), fills tm_wday and tm_yday, and returns the time.
// (time_t)-1 with errno EOVERFLOW when the date is more than about five million years away - the same
// bound gmtime keeps, so what one gives the other takes back. (time_t)-1 is also, as C has it,
// 1969-12-31 23:59:59: errno tells the two apart.
time_t mktime(struct tm* tm);

char*  asctime(const struct tm* tm);                 // "Thu Jan  1 00:00:00 1970\n", in a static buffer
char*  ctime(const time_t* t);

// %Y %C %y %m %d %e %H %I %M %S %p %a %A %b %B %h %j %u %w %Z %z %s %F %T %D %R %c %x %X %n %t %%
// Returns the length written (without the NUL), or 0 when the result does not fit in `max`.
size_t strftime(char* buf, size_t max, const char* fmt, const struct tm* tm);

// The finer clock, in the C11 way. TIME_UTC is the calendar time: the machine's clock register counts whole
// seconds, so tv_nsec is 0 and the resolution is one second. TIME_MONOTONIC (C23) is the time since the machine
// started, to the nanosecond the host clock gives - timer_nanos_resolution() says how fine that is - and is
// the one for measuring a span.
struct timespec
{
    time_t tv_sec;
    long tv_nsec;    // 0..999999999
};

_Static_assert(sizeof(struct timespec) == 16, "a timespec is a time_t and a long, padded to the time_t");

#define TIME_UTC       1
#define TIME_MONOTONIC 2

int timespec_get(struct timespec* ts, int base);      // `base` on success, 0 for a base that is not supported
int timespec_getres(struct timespec* ts, int base);   // the same, for the clock's resolution

// Waits `seconds` of real time, sleeping (timer_wait_ms: the machine halts with the timer's alarm armed, and
// needs no handler). Returns 0.
unsigned int sleep(unsigned int seconds);

// POSIX nanosleep: sleeps for *req the same way, to the nanosecond the clock and the host's sleep allow. Nothing
// interrupts it, so *rem (when not null) is set to 0. -1 with errno EINVAL for a null req, a negative time or
// a tv_nsec outside 0..999999999.
int nanosleep(const struct timespec* req, struct timespec* rem);
```
