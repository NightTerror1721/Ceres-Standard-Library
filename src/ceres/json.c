// JSON tokens and a JSON writer. See ceres/json.h.
#include "ceres/json.h"
#include "ceres/utf8.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "errno.h"
#include "math.h"

int json_error_at;

// ---- reading ----

struct parser
{
    const char* s;
    size_t n, pos;
    struct json_token* t;
    int max, count;
    int error;                  // EINVAL or ENOSPC once something went wrong
};

static int fail(struct parser* p, int e)
{
    if (p->error == 0)
    {
        p->error = e;
        json_error_at = (int)p->pos;
    }
    return -1;
}

static int at_end(const struct parser* p)
{
    return p->pos >= p->n || p->s[p->pos] == 0;
}

static void skip_space(struct parser* p)
{
    while (!at_end(p) && (p->s[p->pos] == ' ' || p->s[p->pos] == '\t' || p->s[p->pos] == '\n' || p->s[p->pos] == '\r'))
        p->pos++;
}

static int new_token(struct parser* p, enum json_type type)
{
    if (p->count >= p->max)
        return fail(p, ENOSPC);
    struct json_token* t = &p->t[p->count];
    t->type = type;
    t->start = (int)p->pos;
    t->end = (int)p->pos;
    t->size = 0;
    t->skip = 1;
    return p->count++;
}

static int hex(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int parse_string(struct parser* p)
{
    p->pos++;                                        // the opening quote
    int i = new_token(p, JSON_STRING);
    if (i < 0)
        return -1;
    for (;;)
    {
        if (at_end(p))
            return fail(p, EINVAL);
        unsigned char c = (unsigned char)p->s[p->pos];
        if (c == '"')
            break;
        if (c < 0x20)
            return fail(p, EINVAL);                  // a control character must be escaped
        if (c == '\\')
        {
            p->pos++;
            if (at_end(p))
                return fail(p, EINVAL);
            char e = p->s[p->pos];
            if (e == 'u')
            {
                for (int k = 1; k <= 4; k++)
                    if (p->pos + (size_t)k >= p->n || hex(p->s[p->pos + (size_t)k]) < 0)
                        return fail(p, EINVAL);
                p->pos += 4;
            }
            else if (strchr("\"\\/bfnrt", e) == 0 || e == 0)
                return fail(p, EINVAL);
        }
        p->pos++;
    }
    p->t[i].end = (int)p->pos;
    p->pos++;                                        // the closing quote
    return i;
}

static int digits(struct parser* p)
{
    size_t from = p->pos;
    while (!at_end(p) && p->s[p->pos] >= '0' && p->s[p->pos] <= '9')
        p->pos++;
    return p->pos > from;
}

static int parse_number(struct parser* p)
{
    int i = new_token(p, JSON_NUMBER);
    if (i < 0)
        return -1;
    if (p->s[p->pos] == '-')
        p->pos++;
    if (!at_end(p) && p->s[p->pos] == '0')
        p->pos++;                                    // a leading zero stands alone
    else if (!digits(p))
        return fail(p, EINVAL);
    if (!at_end(p) && p->s[p->pos] == '.')
    {
        p->pos++;
        if (!digits(p))
            return fail(p, EINVAL);
    }
    if (!at_end(p) && (p->s[p->pos] == 'e' || p->s[p->pos] == 'E'))
    {
        p->pos++;
        if (!at_end(p) && (p->s[p->pos] == '+' || p->s[p->pos] == '-'))
            p->pos++;
        if (!digits(p))
            return fail(p, EINVAL);
    }
    p->t[i].end = (int)p->pos;
    return i;
}

static int parse_word(struct parser* p, const char* word, enum json_type type)
{
    size_t n = strlen(word);
    if (p->n - p->pos < n || memcmp(p->s + p->pos, word, n) != 0)
        return fail(p, EINVAL);
    int i = new_token(p, type);
    if (i < 0)
        return -1;
    p->pos += n;
    p->t[i].end = (int)p->pos;
    return i;
}

static int parse_value(struct parser* p, int depth);

// An object or an array, from its opening bracket.
static int parse_container(struct parser* p, int depth, int object)
{
    if (depth >= JSON_MAX_DEPTH)
        return fail(p, EINVAL);
    int i = new_token(p, object ? JSON_OBJECT : JSON_ARRAY);
    if (i < 0)
        return -1;
    char close = object ? '}' : ']';
    p->pos++;
    skip_space(p);
    if (!at_end(p) && p->s[p->pos] == close)
        p->pos++;
    else
        for (;;)
        {
            skip_space(p);
            if (object)
            {
                if (at_end(p) || p->s[p->pos] != '"' || parse_string(p) < 0)
                    return fail(p, EINVAL);
                skip_space(p);
                if (at_end(p) || p->s[p->pos] != ':')
                    return fail(p, EINVAL);
                p->pos++;
            }
            if (parse_value(p, depth + 1) < 0)
                return -1;
            p->t[i].size++;
            skip_space(p);
            if (at_end(p))
                return fail(p, EINVAL);
            char c = p->s[p->pos++];
            if (c == close)
                break;
            if (c != ',')
            {
                p->pos--;
                return fail(p, EINVAL);
            }
        }
    p->t[i].end = (int)p->pos;
    p->t[i].skip = p->count - i;
    return i;
}

static int parse_value(struct parser* p, int depth)
{
    skip_space(p);
    if (at_end(p))
        return fail(p, EINVAL);
    char c = p->s[p->pos];
    if (c == '{' || c == '[')
        return parse_container(p, depth, c == '{');
    if (c == '"')
        return parse_string(p);
    if (c == '-' || (c >= '0' && c <= '9'))
        return parse_number(p);
    if (c == 't')
        return parse_word(p, "true", JSON_TRUE);
    if (c == 'f')
        return parse_word(p, "false", JSON_FALSE);
    if (c == 'n')
        return parse_word(p, "null", JSON_NULL);
    return fail(p, EINVAL);
}

int json_parse(const char* text, size_t n, struct json_token* t, int max)
{
    struct parser p = { text, n, 0, t, max, 0, 0 };
    json_error_at = -1;
    if (parse_value(&p, 0) >= 0)
    {
        skip_space(&p);
        if (!at_end(&p))
            fail(&p, EINVAL);                        // something after the document
    }
    if (p.error != 0)
    {
        errno = p.error;
        return -1;
    }
    return p.count;
}

// The next character of a string token from *pos, as UTF-8 into out: its bytes, or 0 at the end of the token.
static int unescape_next(const char* text, int* pos, int end, char* out)
{
    if (*pos >= end)
        return 0;
    char c = text[(*pos)++];
    if (c != '\\')
    {
        out[0] = c;
        return 1;
    }
    char e = text[(*pos)++];
    switch (e)
    {
    case 'b': out[0] = '\b'; return 1;
    case 'f': out[0] = '\f'; return 1;
    case 'n': out[0] = '\n'; return 1;
    case 'r': out[0] = '\r'; return 1;
    case 't': out[0] = '\t'; return 1;
    case 'u': break;
    default: out[0] = e; return 1;                   // \" \\ \/
    }
    unsigned int cp = 0;
    for (int k = 0; k < 4; k++)
        cp = cp * 16u + (unsigned int)hex(text[(*pos)++]);
    if (cp >= 0xD800u && cp <= 0xDBFFu && *pos + 6 <= end && text[*pos] == '\\' && text[*pos + 1] == 'u')
    {
        unsigned int low = 0;
        for (int k = 2; k < 6; k++)
            low = low * 16u + (unsigned int)hex(text[*pos + k]);
        if (low >= 0xDC00u && low <= 0xDFFFu)
        {
            *pos += 6;
            cp = 0x10000u + ((cp - 0xD800u) << 10) + (low - 0xDC00u);
        }
    }
    int n = utf8_encode(cp, out);                    // a surrogate left alone is no character
    return n > 0 ? n : utf8_encode(UTF8_REPLACEMENT, out);
}

static int is(const struct json_token* t, int i, enum json_type type)
{
    return i >= 0 && t[i].type == type;
}

int json_get_string(const char* text, const struct json_token* t, int i, char* out, size_t cap)
{
    if (!is(t, i, JSON_STRING) || cap == 0)
        return -1;
    int pos = t[i].start;
    size_t length = 0;
    char unit[UTF8_MAX];
    int n;
    while ((n = unescape_next(text, &pos, t[i].end, unit)) > 0)
    {
        if (length + (size_t)n >= cap)
        {
            out[0] = 0;
            return -1;
        }
        memcpy(out + length, unit, (size_t)n);
        length += (size_t)n;
    }
    out[length] = 0;
    return (int)length;
}

int json_equals(const char* text, const struct json_token* t, int i, const char* s)
{
    if (!is(t, i, JSON_STRING))
        return 0;
    int pos = t[i].start;
    char unit[UTF8_MAX];
    int n;
    while ((n = unescape_next(text, &pos, t[i].end, unit)) > 0)
    {
        if (strncmp(s, unit, (size_t)n) != 0 || memchr(s, 0, (size_t)n) != 0)
            return 0;
        s += n;
    }
    return *s == 0;
}

int json_find(const char* text, const struct json_token* t, int object, const char* key)
{
    if (!is(t, object, JSON_OBJECT))
        return -1;
    int i = object + 1;
    for (int k = 0; k < t[object].size; k++)
    {
        if (json_equals(text, t, i, key))
            return i + 1;
        i += 1 + t[i + 1].skip;                      // the key, then its value
    }
    return -1;
}

int json_index(const struct json_token* t, int array, int i)
{
    if (!is(t, array, JSON_ARRAY) || i < 0 || i >= t[array].size)
        return -1;
    int at = array + 1;
    while (i-- > 0)
        at += t[at].skip;
    return at;
}

// A number token's text, NUL-terminated, into buf: 0, or -1 when it is not one (or too long to be sensible).
static int number_text(const char* text, const struct json_token* t, int i, char* buf, size_t cap)
{
    if (!is(t, i, JSON_NUMBER) || (size_t)(t[i].end - t[i].start) >= cap)
        return -1;
    memcpy(buf, text + t[i].start, (size_t)(t[i].end - t[i].start));
    buf[t[i].end - t[i].start] = 0;
    return 0;
}

long long json_get_int64(const char* text, const struct json_token* t, int i, long long fallback)
{
    char buf[32];
    if (number_text(text, t, i, buf, sizeof buf) != 0 || strpbrk(buf, ".eE") != 0)
        return fallback;
    char* end;
    errno = 0;
    long long v = strtoll(buf, &end, 10);
    return errno == 0 && *end == 0 ? v : fallback;
}

int json_get_int(const char* text, const struct json_token* t, int i, int fallback)
{
    long long v = json_get_int64(text, t, i, (long long)fallback);
    return v >= -2147483647LL - 1 && v <= 2147483647LL ? (int)v : fallback;
}

float json_get_float(const char* text, const struct json_token* t, int i, float fallback)
{
    char buf[64];
    if (number_text(text, t, i, buf, sizeof buf) != 0)
        return fallback;
    return strtof(buf, 0);                           // the grammar was checked: it all is the number
}

int json_get_bool(const struct json_token* t, int i, int fallback)
{
    if (is(t, i, JSON_TRUE))
        return 1;
    if (is(t, i, JSON_FALSE))
        return 0;
    return fallback;
}

int json_is_null(const struct json_token* t, int i)
{
    return is(t, i, JSON_NULL);
}

// ---- writing ----

#define KIND_OBJECT 0x10
#define KIND_ARRAY  0x20
#define WROTE_VALUE 1
#define WROTE_KEY   2

static void emit(struct json_writer* w, const char* s, size_t n)
{
    for (size_t k = 0; k < n; k++, w->len++)
        if (w->len + 1 < w->cap)
            w->buf[w->len] = s[k];                   // one byte kept for the NUL
}

static void emit_str(struct json_writer* w, const char* s)
{
    emit(w, s, strlen(s));
}

void json_writer_init(struct json_writer* w, char* buf, size_t cap)
{
    w->buf = buf;
    w->cap = buf != 0 ? cap : 0;
    w->len = 0;
    w->depth = 0;
    w->failed = 0;
    w->state[0] = 0;
}

// Before a value: the comma, or the check that a key came first. State[0] is the top level, where one value goes.
static void before_value(struct json_writer* w)
{
    unsigned char* s = &w->state[w->depth];
    if (w->depth == 0)
    {
        if (*s & WROTE_VALUE)
            w->failed = 1;                           // a second document
    }
    else if (*s & KIND_OBJECT)
    {
        if ((*s & WROTE_KEY) == 0)
            w->failed = 1;                           // a value with no key
    }
    else if (*s & WROTE_VALUE)
        emit(w, ",", 1);
    *s = (unsigned char)((*s & 0xF0) | WROTE_VALUE);
}

static void open_level(struct json_writer* w, unsigned char kind, const char* bracket)
{
    before_value(w);
    emit(w, bracket, 1);
    if (w->depth + 1 >= JSON_MAX_DEPTH)
    {
        w->failed = 1;
        return;
    }
    w->state[++w->depth] = kind;
}

static void close_level(struct json_writer* w, unsigned char kind, const char* bracket)
{
    if (w->depth == 0 || (w->state[w->depth] & 0xF0) != kind || (w->state[w->depth] & WROTE_KEY))
    {
        w->failed = 1;                               // not open, the other kind, or a key with no value
        return;
    }
    w->depth--;
    emit(w, bracket, 1);
}

void json_object_begin(struct json_writer* w) { open_level(w, KIND_OBJECT, "{"); }
void json_object_end(struct json_writer* w)   { close_level(w, KIND_OBJECT, "}"); }
void json_array_begin(struct json_writer* w)  { open_level(w, KIND_ARRAY, "["); }
void json_array_end(struct json_writer* w)    { close_level(w, KIND_ARRAY, "]"); }

static void quoted(struct json_writer* w, const char* s)
{
    emit(w, "\"", 1);
    for (; s != 0 && *s != 0; s++)
    {
        unsigned char c = (unsigned char)*s;
        char esc[8];
        if (c == '"' || c == '\\')
        {
            esc[0] = '\\';
            esc[1] = (char)c;
            emit(w, esc, 2);
        }
        else if (c < 0x20)
        {
            const char* named = c == '\n' ? "\\n" : c == '\r' ? "\\r" : c == '\t' ? "\\t" : c == '\b' ? "\\b" : c == '\f' ? "\\f" : 0;
            if (named != 0)
                emit_str(w, named);
            else
            {
                snprintf(esc, sizeof esc, "\\u%04x", c);
                emit_str(w, esc);
            }
        }
        else
            emit(w, (const char*)&c, 1);
    }
    emit(w, "\"", 1);
}

void json_key(struct json_writer* w, const char* key)
{
    unsigned char* s = &w->state[w->depth];
    if (w->depth == 0 || (*s & KIND_OBJECT) == 0 || (*s & WROTE_KEY))
    {
        w->failed = 1;
        return;
    }
    if (*s & WROTE_VALUE)
        emit(w, ",", 1);
    quoted(w, key);
    emit(w, ":", 1);
    *s = (unsigned char)((*s & 0xF0) | WROTE_KEY);
}

void json_put_string(struct json_writer* w, const char* s)
{
    before_value(w);
    quoted(w, s);
}

void json_put_int(struct json_writer* w, long long v)
{
    char text[24];
    before_value(w);
    snprintf(text, sizeof text, "%lld", v);
    emit_str(w, text);
}

void json_put_float(struct json_writer* w, float v)
{
    char text[32];
    before_value(w);
    if (isnan(v) || isinf(v))
    {
        emit_str(w, "null");
        return;
    }
    snprintf(text, sizeof text, "%.9g", v);         // nine digits read back as the same float
    emit_str(w, text);
}

void json_put_bool(struct json_writer* w, int v)
{
    before_value(w);
    emit_str(w, v ? "true" : "false");
}

void json_put_null(struct json_writer* w)
{
    before_value(w);
    emit_str(w, "null");
}

int json_writer_end(struct json_writer* w)
{
    if (w->cap > 0)
        w->buf[w->len < w->cap ? w->len : w->cap - 1] = 0;
    if (w->failed || w->depth != 0 || (w->state[0] & WROTE_VALUE) == 0)
    {
        errno = EINVAL;
        return -1;
    }
    if (w->len >= w->cap)
    {
        errno = ENOSPC;
        return -1;
    }
    return (int)w->len;
}
