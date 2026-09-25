#pragma once

#include "../stddef.h"

// JSON, read and written without the heap.
//
// READING turns the text into an array of tokens you give it - one per value, key and element - and leaves the
// text where it is; the getters read a token's value out of it when asked.
//
//   struct json_token t[64];
//   int n = json_parse(text, strlen(text), t, 64);       // {"name": "Ana", "level": 3, "items": [1, 2]}
//   int level = json_get_int(text, t, json_find(text, t, 0, "level"), 1);
//   int items = json_find(text, t, 0, "items");
//   for (int i = 0; i < t[items].size; i++)
//       total += json_get_int(text, t, json_index(t, items, i), 0);
//
// Token 0 is the whole document. An object's tokens are its keys, each followed by its value; `size` counts the
// keys of an object and the elements of an array, and `skip` is how many tokens a value takes, its own included,
// so the next one after it is at i + t[i].skip. A getter given a token of another type, or -1 (what json_find and
// json_index return when there is nothing), gives back the fallback.
//
// The parser follows RFC 8259 strictly (no comments, no trailing commas, no single quotes) and nests 64 deep at
// most. json_parse returns how many tokens it used, or -1 with errno: EINVAL for text that is not JSON (its byte
// offset in *json_error_at), ENOSPC when there are more values than tokens.
//
// WRITING goes into a buffer, commas and quotes taken care of:
//
//   char out[256];
//   struct json_writer w;
//   json_writer_init(&w, out, sizeof out);
//   json_object_begin(&w);
//   json_key(&w, "name");  json_put_string(&w, "Ana");
//   json_key(&w, "items"); json_array_begin(&w); json_put_int(&w, 1); json_put_int(&w, 2); json_array_end(&w);
//   json_object_end(&w);
//   int length = json_writer_end(&w);                    // -1 if it did not fit, or was not whole
//
// A float with no JSON form (NaN, infinity) is written as null. Strings are UTF-8 and go through as they are,
// with " \ and the control characters escaped.

enum json_type { JSON_NONE, JSON_OBJECT, JSON_ARRAY, JSON_STRING, JSON_NUMBER, JSON_TRUE, JSON_FALSE, JSON_NULL };

struct json_token
{
    enum json_type type;
    int start, end;             // its bytes in the text: a string's without the quotes
    int size;                   // an object's keys, an array's elements
    int skip;                   // the tokens it takes, its own included
};

#define JSON_MAX_DEPTH 64

extern int json_error_at;

int json_parse(const char* text, size_t n, struct json_token* t, int max);
int json_find(const char* text, const struct json_token* t, int object, const char* key);   // the value's token, or -1
int json_index(const struct json_token* t, int array, int i);                              // element i's token, or -1

// A string's value, unescaped (\uXXXX as UTF-8), into out with a NUL: its length, or -1 when the token is no
// string or the value does not fit.
int   json_get_string(const char* text, const struct json_token* t, int i, char* out, size_t cap);
int   json_equals(const char* text, const struct json_token* t, int i, const char* s);   // a string with this value
long long json_get_int64(const char* text, const struct json_token* t, int i, long long fallback);   // an integer
int   json_get_int(const char* text, const struct json_token* t, int i, int fallback);    // ... in int's range
float json_get_float(const char* text, const struct json_token* t, int i, float fallback);
int   json_get_bool(const struct json_token* t, int i, int fallback);
int   json_is_null(const struct json_token* t, int i);

struct json_writer
{
    char* buf;
    size_t cap, len;            // len goes on counting past cap, so a failed write says how much it needed
    int depth;
    int failed;
    unsigned char state[JSON_MAX_DEPTH];   // per level: object or array, and whether a value or a key came last
};

void json_writer_init(struct json_writer* w, char* buf, size_t cap);
void json_object_begin(struct json_writer* w);
void json_object_end(struct json_writer* w);
void json_array_begin(struct json_writer* w);
void json_array_end(struct json_writer* w);
void json_key(struct json_writer* w, const char* key);
void json_put_string(struct json_writer* w, const char* s);
void json_put_int(struct json_writer* w, long long v);
void json_put_float(struct json_writer* w, float v);
void json_put_bool(struct json_writer* w, int v);
void json_put_null(struct json_writer* w);
int  json_writer_end(struct json_writer* w);             // the length written, or -1
