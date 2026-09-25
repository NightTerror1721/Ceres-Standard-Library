// INI files. See ceres/ini.h.
#include "ceres/ini.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "strings.h"
#include "ctype.h"
#include "errno.h"

#define LINE_MAX_BYTES 512                           // a longer line is an error, and skipped

static int blank(char c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

// The line [s, end) trimmed at both ends: its start, and its length in *n.
static const char* trim(const char* s, const char* end, size_t* n)
{
    while (s < end && blank(*s))
        s++;
    while (end > s && blank(end[-1]))
        end--;
    *n = (size_t)(end - s);
    return s;
}

// A value from `s` (trimmed at the front) to `end` into out: quoted or not. 0, or -1 when it is not well formed.
static int value_of(const char* s, const char* end, char* out)
{
    size_t n;
    s = trim(s, end, &n);
    end = s + n;
    if (n > 0 && *s == '"')
    {
        const char* p = s + 1;
        size_t o = 0;
        while (p < end && *p != '"')
        {
            char c = *p++;
            if (c == '\\' && p < end)
            {
                char e = *p++;
                c = e == 'n' ? '\n' : e == 't' ? '\t' : e;   // \" \\ and any other as itself
            }
            out[o++] = c;
        }
        if (p >= end)
            return -1;                               // no closing quote
        out[o] = 0;
        p++;
        while (p < end && blank(*p))
            p++;
        return p == end || *p == ';' || *p == '#' ? 0 : -1;   // only a comment after it
    }
    // Unquoted: up to a comment that follows a blank.
    const char* stop = end;
    for (const char* p = s; p < end; p++)
        if ((*p == ';' || *p == '#') && p > s && blank(p[-1]))
        {
            stop = p;
            break;
        }
    s = trim(s, stop, &n);
    memcpy(out, s, n);
    out[n] = 0;
    return 0;
}

int ini_parse(const char* text, ini_visit visit, void* ctx)
{
    char section[LINE_MAX_BYTES];
    char key[LINE_MAX_BYTES];
    char value[LINE_MAX_BYTES];
    section[0] = 0;
    int first_error = 0;
    int line = 0;
    const char* p = text;
    if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF)
        p += 3;                                      // a UTF-8 byte order mark
    while (*p != 0)
    {
        line++;
        const char* end = p;
        while (*end != 0 && *end != '\n')
            end++;
        const char* next = *end == '\n' ? end + 1 : end;
        size_t n;
        const char* s = trim(p, end, &n);
        int bad = 0;
        if (n >= LINE_MAX_BYTES)
            bad = 1;
        else if (n == 0 || *s == ';' || *s == '#')
            ;                                        // blank, or a comment
        else if (*s == '[')
        {
            const char* close = memchr(s, ']', n);
            size_t length;
            const char* name = close != 0 ? trim(s + 1, close, &length) : 0;
            if (name == 0)
                bad = 1;
            else
            {
                memcpy(section, name, length);
                section[length] = 0;
            }
        }
        else
        {
            const char* eq = s;
            while (eq < s + n && *eq != '=' && *eq != ':')
                eq++;
            size_t length;
            const char* name = trim(s, eq, &length);
            if (eq == s + n || length == 0 || value_of(eq + 1, s + n, value) != 0)
                bad = 1;
            else
            {
                memcpy(key, name, length);
                key[length] = 0;
                if (visit(section, key, value, ctx) != 0)
                    return -1;
            }
        }
        if (bad && first_error == 0)
            first_error = line;
        p = next;
    }
    return first_error;
}

// ---- the document ----

void ini_init(struct ini* d)
{
    d->entries = 0;
    d->count = 0;
    d->capacity = 0;
}

void ini_free(struct ini* d)
{
    for (int i = 0; i < d->count; i++)
    {
        free(d->entries[i].section);
        free(d->entries[i].key);
        free(d->entries[i].value);
    }
    free(d->entries);
    ini_init(d);
}

static struct ini_entry* find(const struct ini* d, const char* section, const char* key)
{
    if (section == 0)
        section = "";
    for (int i = 0; i < d->count; i++)
        if (strcasecmp(d->entries[i].section, section) == 0 && strcasecmp(d->entries[i].key, key) == 0)
            return &d->entries[i];
    return 0;
}

static char* copy(const char* s)
{
    size_t n = strlen(s) + 1;
    char* c = (char*)malloc(n);
    if (c != 0)
        memcpy(c, s, n);
    return c;
}

int ini_set(struct ini* d, const char* section, const char* key, const char* value)
{
    if (key == 0 || value == 0)
    {
        errno = EINVAL;
        return -1;
    }
    if (section == 0)
        section = "";
    char* v = copy(value);
    if (v == 0)
    {
        errno = ENOMEM;
        return -1;
    }
    struct ini_entry* e = find(d, section, key);
    if (e != 0)
    {
        free(e->value);
        e->value = v;
        return 0;
    }
    if (d->count == d->capacity)
    {
        int capacity = d->capacity == 0 ? 8 : d->capacity * 2;
        struct ini_entry* more = (struct ini_entry*)realloc(d->entries, (size_t)capacity * sizeof(struct ini_entry));
        if (more == 0)
        {
            free(v);
            errno = ENOMEM;
            return -1;
        }
        d->entries = more;
        d->capacity = capacity;
    }
    e = &d->entries[d->count];
    e->section = copy(section);
    e->key = copy(key);
    e->value = v;
    if (e->section == 0 || e->key == 0)
    {
        free(e->section);
        free(e->key);
        free(v);
        errno = ENOMEM;
        return -1;
    }
    d->count++;
    return 0;
}

int ini_set_int(struct ini* d, const char* section, const char* key, int value)
{
    char text[16];
    snprintf(text, sizeof text, "%d", value);
    return ini_set(d, section, key, text);
}

int ini_set_float(struct ini* d, const char* section, const char* key, float value)
{
    char text[32];
    snprintf(text, sizeof text, "%.9g", value);      // nine digits: the same float reads back
    return ini_set(d, section, key, text);
}

int ini_remove(struct ini* d, const char* section, const char* key)
{
    struct ini_entry* e = find(d, section, key);
    if (e == 0)
    {
        errno = ENOENT;
        return -1;
    }
    free(e->section);
    free(e->key);
    free(e->value);
    int at = (int)(e - d->entries);
    memmove(e, e + 1, (size_t)(d->count - at - 1) * sizeof *e);
    d->count--;
    return 0;
}

static int keep(const char* section, const char* key, const char* value, void* ctx)
{
    return ini_set((struct ini*)ctx, section, key, value) != 0;
}

int ini_read(struct ini* d, const char* text)
{
    return ini_parse(text, keep, d);
}

int ini_load(struct ini* d, const char* path)
{
    FILE* f = fopen(path, "rb");
    if (f == 0)
        return errno == ENOENT ? 0 : -1;
    size_t cap = 1024, n = 0;
    char* text = (char*)malloc(cap);
    while (text != 0)
    {
        if (cap - n < 2)
        {
            char* more = (char*)realloc(text, cap * 2);
            if (more == 0)
            {
                free(text);
                text = 0;
                break;
            }
            text = more;
            cap *= 2;
        }
        size_t got = fread(text + n, 1, cap - n - 1, f);
        n += got;
        if (got == 0)
            break;
    }
    fclose(f);
    if (text == 0)
    {
        errno = ENOMEM;
        return -1;
    }
    text[n] = 0;
    int result = ini_read(d, text);
    free(text);
    return result;
}

const char* ini_get(const struct ini* d, const char* section, const char* key, const char* fallback)
{
    const struct ini_entry* e = find(d, section, key);
    return e != 0 ? e->value : fallback;
}

int ini_get_int(const struct ini* d, const char* section, const char* key, int fallback)
{
    const char* v = ini_get(d, section, key, 0);
    if (v == 0 || *v == 0)
        return fallback;
    char* end;
    errno = 0;
    long n = strtol(v, &end, 0);
    return *end == 0 && errno == 0 ? (int)n : fallback;
}

float ini_get_float(const struct ini* d, const char* section, const char* key, float fallback)
{
    const char* v = ini_get(d, section, key, 0);
    if (v == 0 || *v == 0)
        return fallback;
    char* end;
    float f = strtof(v, &end);
    return *end == 0 ? f : fallback;
}

int ini_get_bool(const struct ini* d, const char* section, const char* key, int fallback)
{
    const char* v = ini_get(d, section, key, 0);
    if (v == 0)
        return fallback;
    static const char* const yes[] = { "yes", "true", "on", "1" };
    static const char* const no[] = { "no", "false", "off", "0" };
    for (int i = 0; i < 4; i++)
    {
        if (strcasecmp(v, yes[i]) == 0)
            return 1;
        if (strcasecmp(v, no[i]) == 0)
            return 0;
    }
    return fallback;
}

// ---- writing ----

// Whether a value has to be quoted to read back as it is.
static int needs_quotes(const char* v)
{
    size_t n = strlen(v);
    if (n == 0)
        return 0;
    if (blank(v[0]) || blank(v[n - 1]) || v[0] == '"')
        return 1;
    for (size_t i = 0; i < n; i++)
        if (v[i] == '\n' || ((v[i] == ';' || v[i] == '#') && i > 0 && blank(v[i - 1])))
            return 1;
    return 0;
}

struct text
{
    char* s;
    size_t n, cap;
    int failed;
};

static void add(struct text* t, const char* s, size_t n)
{
    if (t->failed)
        return;
    if (t->cap - t->n < n + 1)
    {
        size_t cap = t->cap * 2 + n + 64;
        char* more = (char*)realloc(t->s, cap);
        if (more == 0)
        {
            t->failed = 1;
            return;
        }
        t->s = more;
        t->cap = cap;
    }
    memcpy(t->s + t->n, s, n);
    t->n += n;
    t->s[t->n] = 0;
}

static void add_str(struct text* t, const char* s)
{
    add(t, s, strlen(s));
}

char* ini_write(const struct ini* d, size_t* size)
{
    struct text t = { 0, 0, 0, 0 };
    add(&t, "", 0);
    for (int i = 0; i < d->count; i++)
    {
        const char* section = d->entries[i].section;
        int seen = 0;
        for (int j = 0; j < i && !seen; j++)
            seen = strcasecmp(d->entries[j].section, section) == 0;
        if (seen)
            continue;                                // written with the first of its section
        if (section[0] != 0 || i > 0)
        {
            if (t.n > 0)
                add_str(&t, "\n");
            add_str(&t, "[");
            add_str(&t, section);
            add_str(&t, "]\n");
        }
        for (int j = i; j < d->count; j++)
        {
            const struct ini_entry* e = &d->entries[j];
            if (strcasecmp(e->section, section) != 0)
                continue;
            add_str(&t, e->key);
            add_str(&t, e->value[0] != 0 ? " = " : " =");
            if (!needs_quotes(e->value))
                add_str(&t, e->value);
            else
            {
                add_str(&t, "\"");
                for (const char* v = e->value; *v != 0; v++)
                {
                    if (*v == '"' || *v == '\\')
                        add(&t, "\\", 1);
                    if (*v == '\n')
                        add_str(&t, "\\n");
                    else if (*v == '\t')
                        add_str(&t, "\\t");
                    else
                        add(&t, v, 1);
                }
                add_str(&t, "\"");
            }
            add_str(&t, "\n");
        }
    }
    if (t.failed)
    {
        free(t.s);
        errno = ENOMEM;
        return 0;
    }
    if (size != 0)
        *size = t.n;
    return t.s;
}

int ini_save(const struct ini* d, const char* path)
{
    size_t n;
    char* text = ini_write(d, &n);
    if (text == 0)
        return -1;
    FILE* f = fopen(path, "wb");
    int ok = f != 0 && fwrite(text, 1, n, f) == n;
    if (f != 0 && fclose(f) != 0)
        ok = 0;
    free(text);
    return ok ? 0 : -1;
}
