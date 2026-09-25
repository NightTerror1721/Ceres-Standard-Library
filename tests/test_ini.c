// INI settings (ceres/ini.h): the syntax read line by line, the values kept and typed, changed and written back
// out, and the text written reading back the same.
#include "ceres/test.h"
#include "ceres/ini.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"
#include "errno.h"

static const char* settings =
    "\xEF\xBB\xBF; the game's settings\n"
    "top = level\n"
    "\n"
    "[Video]\r\n"
    "  scale = 3   \n"
    "fullscreen: yes ; a comment after a space\n"
    "ratio = 1.5\n"
    "mask = 0x1F\n"
    "# another comment\n"
    "[player]\n"
    "name = \"Ana Mar\xC3\xAD" "a ; #1\"   ; the quotes keep all that\n"
    "motto = \"say \\\"hi\\\"\\n\"\n"
    "url = http://x.org/#top\n"
    "this line has no equals sign\n"
    "[video]\n"
    "scale = 4\n"
    "empty =\n";

static int visits;

static int count(const char* section, const char* key, const char* value, void* ctx)
{
    visits++;
    return strcmp(key, "ratio") == 0 && ctx != 0;                  // with a ctx, stop at ratio
}

int main(void)
{
    TEST_SECTION("reading");
    CHECK_EQ(ini_parse(settings, count, 0), 14);                     // line 14 is not a setting
    CHECK_EQ(visits, 10);
    visits = 0;
    CHECK_EQ(ini_parse(settings, count, &visits), -1);               // stopped
    CHECK_EQ(visits, 4);

    struct ini cfg;
    ini_init(&cfg);
    CHECK_EQ(ini_read(&cfg, settings), 14);
    CHECK_STR(ini_get(&cfg, "", "top", "?"), "level");
    CHECK_STR(ini_get(&cfg, 0, "top", "?"), "level");
    CHECK_EQ(ini_get_int(&cfg, "VIDEO", "scale", 1), 4);             // the last one given; any case
    CHECK_EQ(ini_get_bool(&cfg, "video", "fullscreen", 0), 1);
    CHECK(ini_get_float(&cfg, "video", "ratio", 0.0f) == 1.5f);
    CHECK_EQ(ini_get_int(&cfg, "video", "mask", 0), 31);
    CHECK_EQ(ini_get_int(&cfg, "video", "fullscreen", 7), 7);       // not a number
    CHECK_EQ(ini_get_bool(&cfg, "video", "scale", 9), 9);           // not a yes or no
    CHECK_STR(ini_get(&cfg, "player", "name", "?"), "Ana Mar\xC3\xAD" "a ; #1");
    CHECK_STR(ini_get(&cfg, "player", "motto", "?"), "say \"hi\"\n");
    CHECK_STR(ini_get(&cfg, "player", "url", "?"), "http://x.org/#top");   // no blank before the #
    CHECK_STR(ini_get(&cfg, "video", "empty", "?"), "");
    CHECK(ini_get(&cfg, "player", "age", 0) == 0);
    CHECK_EQ(ini_get_int(&cfg, "sound", "volume", 80), 80);

    char longline[700];
    memset(longline, 'k', sizeof longline);
    strcpy(longline + 600, " = v\n[unclosed\nq = \"open\nr = \"x\" y\nok = 1\n");
    struct ini broken;
    ini_init(&broken);
    CHECK_EQ(ini_read(&broken, longline), 1);                        // the first bad line: too long
    CHECK_EQ(broken.count, 1);                                       // only ok survives
    CHECK_STR(ini_get(&broken, "", "ok", "?"), "1");
    CHECK(ini_get(&broken, "", 0, "none") != 0);                     // no key: the fallback, not a crash
    ini_free(&broken);
    CHECK_EQ(ini_set(&cfg, "bad]", "k", "v"), -1);                   // would not read back
    CHECK_EQ(ini_set(&cfg, "s", "a=b", "v"), -1);
    CHECK_EQ(ini_set(&cfg, "s", " padded", "v"), -1);
    CHECK_EQ(ini_set(&cfg, "s", "", "v"), -1);
    {
        static char long_value[600];
        memset(long_value, 'x', 505);
        CHECK_EQ(ini_set(&cfg, "s", "k", long_value), 0);            // "k = " and 505: 509 bytes, it reads back
        long_value[505] = 'x';
        long_value[506] = 'x';
        long_value[507] = 'x';
        errno = 0;
        CHECK_EQ(ini_set(&cfg, "s", "k", long_value), -1);           // 512: a reader would drop the line
        CHECK_EQ(errno, EINVAL);
        CHECK_EQ(ini_remove(&cfg, "s", "k"), 0);
    }
    ini_init(&broken);
    ini_read(&broken, "n = 010\nh = -0x10\nq = \" 0x10\"\n");
    CHECK_EQ(ini_get_int(&broken, "", "n", 0), 10);                  // decimal, not octal
    CHECK_EQ(ini_get_int(&broken, "", "h", 0), -16);
    CHECK_EQ(ini_get_int(&broken, "", "q", 0), 16);                  // a quoted value keeps its blank; still hex
    ini_free(&broken);

    TEST_SECTION("changing and writing");
    CHECK_EQ(ini_set_int(&cfg, "video", "scale", 2), 0);
    CHECK_EQ(ini_set_float(&cfg, "video", "ratio", 0.1f), 0);
    CHECK_EQ(ini_set(&cfg, "sound", "volume", "  loud  "), 0);
    CHECK_EQ(ini_remove(&cfg, "video", "mask"), 0);
    CHECK_EQ(ini_remove(&cfg, "video", "mask"), -1);
    size_t size;
    char* text = ini_write(&cfg, &size);
    CHECK(text != 0 && strlen(text) == size);
    printf("%s", text);
    struct ini again;
    ini_init(&again);
    CHECK_EQ(ini_read(&again, text), 0);
    CHECK_EQ(again.count, cfg.count);
    int same = 1;
    for (int i = 0; i < cfg.count; i++)
        same = same && strcmp(ini_get(&again, cfg.entries[i].section, cfg.entries[i].key, "?"), cfg.entries[i].value) == 0;
    CHECK(same);
    CHECK(ini_get_float(&again, "video", "ratio", 0.0f) == 0.1f);
    free(text);
    ini_free(&again);

    TEST_SECTION("files");
    CHECK_EQ(ini_save(&cfg, "host:settings.ini"), 0);
    ini_init(&again);
    CHECK_EQ(ini_load(&again, "host:settings.ini"), 0);
    CHECK_EQ(ini_get_int(&again, "video", "scale", 0), 2);
    ini_free(&again);
    ini_init(&again);
    CHECK_EQ(ini_load(&again, "host:none.ini"), 0);                 // no file: no settings
    CHECK_EQ(again.count, 0);
    ini_free(&again);
    ini_free(&cfg);
    return test_summary();
}
