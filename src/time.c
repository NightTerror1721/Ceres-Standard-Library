#include "time.h"
#include "stdio.h"
#include "string.h"
#include "stdint.h"
#include "ceres/timer.h"
#include "errno.h"

// ---- clocks ----

time_t time(time_t* out)
{
    time_t t = timer_clock();
    if (out != 0)
        *out = t;
    return t;
}

clock_t clock(void)
{
    return (clock_t)(timer_nanos64() / 1000u);
}

int timespec_get(struct timespec* ts, int base)
{
    if (ts == 0)
        return 0;
    if (base == TIME_UTC)
    {
        ts->tv_sec = timer_clock();
        ts->tv_nsec = 0;
        return base;
    }
    if (base == TIME_MONOTONIC)
    {
        // The clock is a real 64-bit nanosecond count now, so one divide splits it into whole seconds
        // and the nanoseconds left over.
        uint64_t total = timer_nanos64();
        ts->tv_sec = (time_t)(total / 1000000000ULL);
        ts->tv_nsec = (long)(total % 1000000000ULL);
        return base;
    }
    return 0;
}

int timespec_getres(struct timespec* ts, int base)
{
    if (ts == 0)
        return 0;
    if (base == TIME_UTC)
    {
        ts->tv_sec = 1;
        ts->tv_nsec = 0;
        return base;
    }
    if (base == TIME_MONOTONIC)
    {
        unsigned int step = timer_nanos_resolution();
        ts->tv_sec = step / 1000000000u;
        ts->tv_nsec = (long)(step % 1000000000u);
        return base;
    }
    return 0;
}

float difftime(time_t end, time_t start)
{
    return (float)(end - start);
}

unsigned int sleep(unsigned int seconds)
{
    if (seconds > 4294967u)
        seconds = 4294967u;                              // the most whole seconds a millisecond count can hold
    timer_wait_ms(seconds * 1000u);                      // exact, where the seconds register was up to a second early
    return 0;
}

int nanosleep(const struct timespec* req, struct timespec* rem)
{
    if (req == 0 || req->tv_sec < 0 || req->tv_nsec < 0 || req->tv_nsec > 999999999)
    {
        errno = EINVAL;
        return -1;
    }
    // Past about 584 years the sum would wrap: a sleep that long ends when the clock does.
    uint64_t now = timer_nanos64();
    uint64_t span = (uint64_t)req->tv_sec > 18000000000ull ? ~(uint64_t)0
                  : (uint64_t)req->tv_sec * 1000000000ull + (uint64_t)req->tv_nsec;
    timer_wait_until_ns64(span > ~(uint64_t)0 - now ? ~(uint64_t)0 : now + span);
    if (rem != 0)
    {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
    }
    return 0;
}

// ---- the calendar ----
// Days <-> civil dates by Howard Hinnant's algorithms: exact for the whole proleptic Gregorian calendar
// with 32-bit integers, a "year" running from March so the leap day is last.

static void civil_from_days(int z, int* year, int* month, int* day)
{
    z += 719468;                                         // days from 0000-03-01
    int era = (z >= 0 ? z : z - 146096) / 146097;
    int doe = z - era * 146097;                          // day of era, 0..146096
    int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int y = yoe + era * 400;
    int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);   // day of the March-based year, 0..365
    int mp = (5 * doy + 2) / 153;
    *day = doy - (153 * mp + 2) / 5 + 1;
    *month = mp < 10 ? mp + 3 : mp - 9;
    *year = y + (*month <= 2 ? 1 : 0);
}

static int days_from_civil(int y, int m, int d)
{
    y -= (m <= 2) ? 1 : 0;
    int era = (y >= 0 ? y : y - 399) / 400;
    int yoe = y - era * 400;
    int doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static int floor_div(int a, int b)
{
    int q = a / b;
    return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
}

static long long floor_div64(long long a, long long b)
{
    long long q = a / b;
    return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
}

// The days either side of 1970 the calendar works with: an int, with room for the algorithms' own offsets
// (about 5.4 million years).
#define MAX_DAYS 2000000000LL

struct tm* gmtime_r(const time_t* t, struct tm* out)
{
    long long days64 = floor_div64(*t, 86400);
    if (days64 > MAX_DAYS || days64 < -MAX_DAYS)
    {
        errno = EOVERFLOW;
        return 0;
    }
    int days = (int)days64;
    int rest = (int)(*t - days64 * 86400);               // 0..86399, before 1970 as after
    out->tm_hour = rest / 3600;
    out->tm_min = (rest % 3600) / 60;
    out->tm_sec = rest % 60;
    out->tm_wday = ((days + 4) % 7 + 7) % 7;             // 1970-01-01 was a Thursday
    int year, month, day;
    civil_from_days(days, &year, &month, &day);
    out->tm_year = year - 1900;
    out->tm_mon = month - 1;
    out->tm_mday = day;
    out->tm_yday = days - days_from_civil(year, 1, 1);
    out->tm_isdst = 0;
    return out;
}

static struct tm shared_tm;

struct tm* gmtime(const time_t* t)          { return gmtime_r(t, &shared_tm); }
struct tm* localtime(const time_t* t)       { return gmtime_r(t, &shared_tm); }
struct tm* localtime_r(const time_t* t, struct tm* out) { return gmtime_r(t, out); }

time_t mktime(struct tm* tm)
{
    long long year = (long long)tm->tm_year + 1900;
    int mon = tm->tm_mon;
    year += floor_div(mon, 12);
    mon -= floor_div(mon, 12) * 12;                      // 0..11
    if (year > 5800000 || year < -5800000)               // an int's worth of days, and a little more: MAX_DAYS decides
    {
        errno = EOVERFLOW;
        return (time_t)-1;
    }

    // Everything below a day is folded into a second-of-day and a number of whole days to carry.
    long long seconds = (long long)tm->tm_hour * 3600 + (long long)tm->tm_min * 60 + tm->tm_sec;
    long long carry = floor_div64(seconds, 86400);
    long long second_of_day = seconds - carry * 86400;
    long long days = (long long)days_from_civil((int)year, mon + 1, 1) + ((long long)tm->tm_mday - 1) + carry;
    if (days > MAX_DAYS || days < -MAX_DAYS)
    {
        errno = EOVERFLOW;
        return (time_t)-1;
    }

    time_t t = days * 86400 + second_of_day;
    gmtime_r(&t, tm);                                    // normalize the fields and fill wday/yday
    return t;
}

// ---- text ----

static const char* const day_names[7] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
static const char* const month_names[12] = { "January", "February", "March", "April", "May", "June", "July",
                                             "August", "September", "October", "November", "December" };

static char ascbuf[64];

char* asctime(const struct tm* tm)
{
    snprintf(ascbuf, sizeof(ascbuf), "%.3s %.3s%3d %.2d:%.2d:%.2d %d\n",
             day_names[(unsigned int)tm->tm_wday % 7u], month_names[(unsigned int)tm->tm_mon % 12u],
             tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_year + 1900);
    return ascbuf;
}

char* ctime(const time_t* t)
{
    struct tm* tm = localtime(t);
    if (tm == 0)
        return 0;                                        // a year an int cannot hold
    return asctime(tm);
}

struct out
{
    char* buf;
    size_t max;
    size_t len;
    int full;
};

static void put_char(struct out* o, char c)
{
    if (o->len + 1 >= o->max)
    {
        o->full = 1;                                      // no room left for it AND the NUL
        return;
    }
    o->buf[o->len] = c;
    o->len++;
}

static void put_text(struct out* o, const char* s)
{
    for (int i = 0; s[i] != 0; i++)
        put_char(o, s[i]);
}

// A non-negative number in at least `width` places, padded with `pad`.
static void put_number(struct out* o, int value, int width, char pad)
{
    char digits[12];
    int n = 0;
    unsigned int v = value < 0 ? 0u : (unsigned int)value;
    do
    {
        digits[n] = (char)('0' + v % 10u);
        n++;
        v /= 10u;
    } while (v != 0);
    for (int i = n; i < width; i++)
        put_char(o, pad);
    for (int i = n - 1; i >= 0; i--)
        put_char(o, digits[i]);
}

static void format_into(struct out* o, const char* fmt, const struct tm* tm);

static void format_one(struct out* o, char spec, const struct tm* tm)
{
    int hour12 = tm->tm_hour % 12;
    if (hour12 == 0)
        hour12 = 12;
    const char* wday = day_names[(unsigned int)tm->tm_wday % 7u];
    const char* month = month_names[(unsigned int)tm->tm_mon % 12u];
    switch (spec)
    {
    case 'Y': put_number(o, tm->tm_year + 1900, 1, '0'); break;
    case 'C': put_number(o, (tm->tm_year + 1900) / 100, 2, '0'); break;
    case 'y': put_number(o, (tm->tm_year + 1900) % 100, 2, '0'); break;
    case 'm': put_number(o, tm->tm_mon + 1, 2, '0'); break;
    case 'd': put_number(o, tm->tm_mday, 2, '0'); break;
    case 'e': put_number(o, tm->tm_mday, 2, ' '); break;
    case 'H': put_number(o, tm->tm_hour, 2, '0'); break;
    case 'I': put_number(o, hour12, 2, '0'); break;
    case 'M': put_number(o, tm->tm_min, 2, '0'); break;
    case 'S': put_number(o, tm->tm_sec, 2, '0'); break;
    case 'p': put_text(o, tm->tm_hour < 12 ? "AM" : "PM"); break;
    case 'a': for (int i = 0; i < 3; i++) put_char(o, wday[i]); break;
    case 'A': put_text(o, wday); break;
    case 'b':
    case 'h': for (int i = 0; i < 3; i++) put_char(o, month[i]); break;
    case 'B': put_text(o, month); break;
    case 'j': put_number(o, tm->tm_yday + 1, 3, '0'); break;
    case 'u': put_number(o, tm->tm_wday == 0 ? 7 : tm->tm_wday, 1, '0'); break;
    case 'w': put_number(o, tm->tm_wday, 1, '0'); break;
    case 'Z': put_text(o, "UTC"); break;
    case 'z': put_text(o, "+0000"); break;
    case 's':
    {
        struct tm copy = *tm;
        time_t t = mktime(&copy);
        char text[24];
        snprintf(text, sizeof(text), "%lld", t);
        put_text(o, text);
        break;
    }
    case 'F': format_into(o, "%Y-%m-%d", tm); break;
    case 'T': format_into(o, "%H:%M:%S", tm); break;
    case 'D': format_into(o, "%m/%d/%y", tm); break;
    case 'R': format_into(o, "%H:%M", tm); break;
    case 'c': format_into(o, "%a %b %e %H:%M:%S %Y", tm); break;
    case 'x': format_into(o, "%m/%d/%y", tm); break;
    case 'X': format_into(o, "%H:%M:%S", tm); break;
    case 'n': put_char(o, '\n'); break;
    case 't': put_char(o, '\t'); break;
    case '%': put_char(o, '%'); break;
    default:  put_char(o, '%'); put_char(o, spec); break;   // unknown: printed back
    }
}

static void format_into(struct out* o, const char* fmt, const struct tm* tm)
{
    for (int i = 0; fmt[i] != 0; i++)
    {
        if (fmt[i] != '%' || fmt[i + 1] == 0)
        {
            put_char(o, fmt[i]);
            continue;
        }
        i++;
        format_one(o, fmt[i], tm);
    }
}

size_t strftime(char* buf, size_t max, const char* fmt, const struct tm* tm)
{
    if (max == 0)
        return 0;
    struct out o;
    o.buf = buf;
    o.max = max;
    o.len = 0;
    o.full = 0;
    format_into(&o, fmt, tm);
    if (o.full)
    {
        buf[0] = 0;
        return 0;
    }
    buf[o.len] = 0;
    return o.len;
}
