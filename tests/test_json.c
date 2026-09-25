// JSON (ceres/json.h): a document into tokens and its values out of them, what RFC 8259 does not allow refused
// with the place it went wrong, and the writer's text reading back as what was written.
#include "ceres/test.h"
#include "ceres/json.h"
#include "string.h"
#include "errno.h"
#include "math.h"

static struct json_token t[64];

static int parses(const char* text)
{
    return json_parse(text, strlen(text), t, 64) >= 0;
}

int main(void)
{
    TEST_SECTION("reading");
    const char* doc =
        "{ \"name\": \"Ana \\\"la\\\" Mar\\u00eda\", \"level\": 12, \"ratio\": -1.5e2,\n"
        "  \"items\": [1, [2, 3], {\"x\": null}, true],\n"
        "  \"big\": 9007199254740993, \"emoji\": \"\\ud83d\\ude00\", \"lone\": \"\\ud800!\",\n"
        "  \"ok\": false, \"empty\": {}, \"none\": [] }";
    int n = json_parse(doc, strlen(doc), t, 64);
    CHECK_EQ(n, 29);
    CHECK_EQ(t[0].type, JSON_OBJECT);
    CHECK_EQ(t[0].size, 10);
    CHECK_EQ(t[0].skip, n);
    char s[40];
    CHECK_EQ(json_get_string(doc, t, json_find(doc, t, 0, "name"), s, sizeof s), 15);
    CHECK_STR(s, "Ana \"la\" Mar\xC3\xAD" "a");
    CHECK_EQ(json_get_int(doc, t, json_find(doc, t, 0, "level"), 0), 12);
    CHECK(json_get_float(doc, t, json_find(doc, t, 0, "ratio"), 0.0f) == -150.0f);
    CHECK_EQ(json_get_int(doc, t, json_find(doc, t, 0, "ratio"), 7), 7);           // not an integer
    CHECK(json_get_int64(doc, t, json_find(doc, t, 0, "big"), 0) == 9007199254740993LL);
    CHECK_EQ(json_get_int(doc, t, json_find(doc, t, 0, "big"), -1), -1);           // past int
    CHECK_EQ(json_get_string(doc, t, json_find(doc, t, 0, "emoji"), s, sizeof s), 4);
    CHECK_STR(s, "\xF0\x9F\x98\x80");                                              // a surrogate pair
    json_get_string(doc, t, json_find(doc, t, 0, "lone"), s, sizeof s);
    CHECK_STR(s, "\xEF\xBF\xBD!");                                                 // alone: U+FFFD
    int items = json_find(doc, t, 0, "items");
    CHECK_EQ(t[items].size, 4);
    CHECK_EQ(json_get_int(doc, t, json_index(t, items, 0), 0), 1);
    int inner = json_index(t, items, 1);
    CHECK_EQ(json_get_int(doc, t, json_index(t, inner, 1), 0), 3);
    CHECK(json_is_null(t, json_find(doc, t, json_index(t, items, 2), "x")));
    CHECK_EQ(json_get_bool(t, json_index(t, items, 3), 0), 1);
    CHECK_EQ(json_index(t, items, 4), -1);
    CHECK_EQ(json_get_bool(t, json_find(doc, t, 0, "ok"), 1), 0);
    CHECK_EQ(t[json_find(doc, t, 0, "empty")].size, 0);
    CHECK_EQ(t[json_find(doc, t, 0, "none")].type, JSON_ARRAY);
    CHECK_EQ(json_find(doc, t, 0, "missing"), -1);
    CHECK_EQ(json_get_int(doc, t, -1, 42), 42);                                    // nothing found: the fallback
    CHECK_EQ(json_get_string(doc, t, json_find(doc, t, 0, "name"), s, 5), -1);   // does not fit
    CHECK(json_equals(doc, t, json_find(doc, t, 0, "name"), "Ana \"la\" Mar\xC3\xAD" "a"));
    CHECK(!json_equals(doc, t, json_find(doc, t, 0, "name"), "Ana"));

    TEST_SECTION("what is not JSON");
    CHECK(parses("  [1, 2.5, -0, 1e10, \"\"]  "));
    CHECK(parses("\"just a string\""));
    CHECK(!parses("[1, 2,]"));                                   // a trailing comma
    CHECK(!parses("{'a': 1}"));
    CHECK(!parses("{\"a\" 1}"));
    CHECK(!parses("[01]"));                                      // a leading zero
    CHECK(!parses("[1.]"));
    CHECK(!parses("[.5]"));
    CHECK(!parses("[tru]"));
    CHECK(!parses("\"a\tb\""));                                  // a raw control character
    CHECK(!parses("\"\\x\""));
    CHECK(!parses("\"\\u12g4\""));
    CHECK(!parses("[1] [2]"));
    CHECK(!parses(""));
    errno = 0;
    CHECK_EQ(json_parse("{\"a\": [1, 2 3]}", 15, t, 64), -1);
    CHECK_EQ(errno, EINVAL);
    CHECK_EQ(json_error_at, 12);                                 // at the 3
    errno = 0;
    CHECK_EQ(json_parse("[1, 2, 3]", 9, t, 3), -1);               // four tokens
    CHECK_EQ(errno, ENOSPC);
    char deep[200];
    for (int i = 0; i < 70; i++) deep[i] = '[';
    for (int i = 0; i < 70; i++) deep[70 + i] = ']';
    deep[140] = 0;
    CHECK(!parses(deep));                                        // deeper than 64
    CHECK_EQ(json_parse("[1]xyz", 3, t, 64), 2);                 // n bytes, whatever follows

    TEST_SECTION("writing");
    char out[256];
    struct json_writer w;
    json_writer_init(&w, out, sizeof out);
    json_object_begin(&w);
    json_key(&w, "name");
    json_put_string(&w, "tab\there \"q\" \x01 \xC3\xB1");
    json_key(&w, "scores");
    json_array_begin(&w);
    json_put_int(&w, 3);
    json_put_int(&w, -9000000000LL);
    json_put_float(&w, 0.1f);
    json_put_float(&w, NAN);
    json_array_end(&w);
    json_key(&w, "ok");
    json_put_bool(&w, 1);
    json_key(&w, "nothing");
    json_put_null(&w);
    json_key(&w, "empty");
    json_object_begin(&w);
    json_object_end(&w);
    json_object_end(&w);
    int length = json_writer_end(&w);
    CHECK_STR(out, "{\"name\":\"tab\\there \\\"q\\\" \\u0001 \xC3\xB1\",\"scores\":[3,-9000000000,0.100000001,null],"
                   "\"ok\":true,\"nothing\":null,\"empty\":{}}");
    CHECK_EQ(length, (int)strlen(out));
    CHECK(json_parse(out, (size_t)length, t, 64) > 0);           // and it reads back
    CHECK_EQ(json_get_string(out, t, json_find(out, t, 0, "name"), s, sizeof s), 17);
    CHECK_STR(s, "tab\there \"q\" \x01 \xC3\xB1");
    CHECK(json_get_float(out, t, json_index(t, json_find(out, t, 0, "scores"), 2), 0.0f) == 0.1f);

    json_writer_init(&w, out, 8);
    json_array_begin(&w);
    json_put_string(&w, "too long for eight");
    json_array_end(&w);
    errno = 0;
    CHECK_EQ(json_writer_end(&w), -1);
    CHECK_EQ(errno, ENOSPC);
    CHECK_EQ((int)w.len, 22);                                    // what it would have needed
    CHECK_EQ((int)strlen(out), 7);                               // what fitted, terminated
    json_writer_init(&w, 0, 0);                                  // just measuring
    json_put_int(&w, 12345);
    CHECK_EQ((int)w.len, 5);
    json_writer_init(&w, out, sizeof out);
    json_object_begin(&w);
    json_put_int(&w, 1);                                         // a value with no key
    json_object_end(&w);
    CHECK_EQ(json_writer_end(&w), -1);
    json_writer_init(&w, out, sizeof out);
    json_array_begin(&w);
    json_object_end(&w);                                         // the wrong bracket
    CHECK_EQ(json_writer_end(&w), -1);
    json_writer_init(&w, out, sizeof out);
    json_put_int(&w, 1);
    json_put_int(&w, 2);                                         // two documents
    CHECK_EQ(json_writer_end(&w), -1);
    return test_summary();
}
