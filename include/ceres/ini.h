#pragma once

#include "../stddef.h"

// INI files, for settings: sections of `key = value` lines.
//
//   ; the game's settings
//   [video]
//   scale = 3
//   fullscreen = yes
//   [player]
//   name = "Ana María"        ; quoted, it keeps its spaces, ; and #
//
//   struct ini cfg;
//   ini_init(&cfg);
//   ini_load(&cfg, "settings.ini");                    // a missing file is just no settings
//   int scale = ini_get_int(&cfg, "video", "scale", 2);
//   ini_set_int(&cfg, "video", "scale", scale + 1);
//   ini_save(&cfg, "settings.ini");
//   ini_free(&cfg);
//
// The syntax: blank lines and lines starting with ; or # are comments; [name] starts a section (keys before the
// first belong to section ""); key = value (or key: value) with the spaces around both trimmed. A value may be
// in double quotes, with \" \\ \n \t inside; an unquoted one ends at a ; or # that follows a space. Section and
// key names compare without regard to case. A key given twice keeps its last value. The text is UTF-8, untouched.
//
// ini_parse() hands each value to a function as it is read, keeping nothing; the struct ini keeps them all, and
// writes them back grouped by section in the order they first came (comments are not kept).

// The line number of the first line that could not be read (0 when there was none); every other line is still
// read. visit returns 0 to go on, anything else to stop there (ini_parse then returns -1).
typedef int (*ini_visit)(const char* section, const char* key, const char* value, void* ctx);
int ini_parse(const char* text, ini_visit visit, void* ctx);

struct ini_entry
{
    char* section;
    char* key;
    char* value;
};

struct ini
{
    struct ini_entry* entries;
    int count;
    int capacity;
};

void ini_init(struct ini* d);
void ini_free(struct ini* d);
int  ini_read(struct ini* d, const char* text);         // as ini_parse, into d; -1 (ENOMEM)
int  ini_load(struct ini* d, const char* path);         // a file into d: as ini_read; a missing file is 0
const char* ini_get(const struct ini* d, const char* section, const char* key, const char* fallback);
int   ini_get_int(const struct ini* d, const char* section, const char* key, int fallback);     // decimal or 0x
float ini_get_float(const struct ini* d, const char* section, const char* key, float fallback);
int   ini_get_bool(const struct ini* d, const char* section, const char* key, int fallback);    // yes/no, true/false, on/off, 1/0
int  ini_set(struct ini* d, const char* section, const char* key, const char* value);   // 0, or -1 (ENOMEM)
int  ini_set_int(struct ini* d, const char* section, const char* key, int value);
int  ini_set_float(struct ini* d, const char* section, const char* key, float value);
int  ini_remove(struct ini* d, const char* section, const char* key);  // 0, or -1 (ENOENT)
char* ini_write(const struct ini* d, size_t* size);      // the text, malloc'd
int  ini_save(const struct ini* d, const char* path);    // 0, or -1
