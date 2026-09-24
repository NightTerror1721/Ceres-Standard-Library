// The program's arguments and environment: what `ceres run prog.cres --env NAME=value -- a b` started it with
// (CeresASM 9c6afbb). The loader placed the strings and the argv/envp arrays at the top of the stack and handed
// them to main in r0-r2; the system control device reads the same three values back, which is how this file
// reaches them without main passing them on.
//
// getenv reads the environment as it came. setenv and unsetenv work on a copy, made the first time either is
// called: its array and every string they add come from malloc, and the strings that came with the program
// stay where the loader put them.

#include "stdlib.h"
#include "string.h"
#include "errno.h"
#include "ceres/sys.h"
#include "ceres.h"

static unsigned int read_register(unsigned int address)
{
    unsigned int value = *(volatile unsigned int*)address;
    return value == 0xFFFFFFFFu ? 0u : value;            // a machine without the registers
}

int sys_argc(void)
{
    return (int)read_register(SYS_CTRL_ARGC);
}

char** sys_argv(void)
{
    return (char**)read_register(SYS_CTRL_ARGV);
}

char** sys_envp(void)
{
    return (char**)read_register(SYS_CTRL_ENVP);
}

static char** owned = 0;          // the copy setenv/unsetenv edit; 0 until one of them is called
static int owned_count = 0;
static int owned_cap = 0;

static char** environment(void)
{
    return owned != 0 ? owned : sys_envp();
}

// The entry for `name` ("NAME=value"), or 0. `index`, when not 0, receives its position.
static char* find(const char* name, int* index)
{
    char** env = environment();
    if (env == 0 || name == 0)
        return 0;
    size_t length = strlen(name);
    for (int i = 0; env[i] != 0; i++)
    {
        if (strncmp(env[i], name, length) == 0 && env[i][length] == '=')
        {
            if (index != 0)
                *index = i;
            return env[i];
        }
    }
    return 0;
}

char* getenv(const char* name)
{
    char* entry = find(name, 0);
    return entry == 0 ? 0 : entry + strlen(name) + 1;
}

static int valid_name(const char* name)
{
    if (name == 0 || name[0] == 0 || strchr(name, '=') != 0)
    {
        errno = EINVAL;
        return 0;
    }
    return 1;
}

// Makes the copy the first time, with room for one more entry.
static int take_ownership(void)
{
    if (owned == 0)
    {
        char** env = sys_envp();
        int count = 0;
        while (env != 0 && env[count] != 0)
            count++;
        owned = (char**)malloc(sizeof(char*) * (size_t)(count + 8));
        if (owned == 0)
        {
            errno = ENOMEM;
            return 0;
        }
        for (int i = 0; i < count; i++)
            owned[i] = env[i];
        owned[count] = 0;
        owned_count = count;
        owned_cap = count + 8;
    }
    if (owned_count + 1 >= owned_cap)
    {
        char** bigger = (char**)realloc(owned, sizeof(char*) * (size_t)(owned_cap * 2));
        if (bigger == 0)
        {
            errno = ENOMEM;
            return 0;
        }
        owned = bigger;
        owned_cap *= 2;
    }
    return 1;
}

int setenv(const char* name, const char* value, int overwrite)
{
    if (!valid_name(name))
        return -1;
    int at = -1;
    if (find(name, &at) != 0 && !overwrite)
        return 0;
    if (!take_ownership())
        return -1;
    size_t name_length = strlen(name);
    size_t value_length = value != 0 ? strlen(value) : 0;
    char* entry = (char*)malloc(name_length + value_length + 2);
    if (entry == 0)
    {
        errno = ENOMEM;
        return -1;
    }
    memcpy(entry, name, name_length);
    entry[name_length] = '=';
    if (value_length != 0)
        memcpy(entry + name_length + 1, value, value_length);
    entry[name_length + 1 + value_length] = 0;
    if (at >= 0)
    {
        owned[at] = entry;          // the old string may be the loader's: it is left where it is
    }
    else
    {
        owned[owned_count++] = entry;
        owned[owned_count] = 0;
    }
    return 0;
}

int unsetenv(const char* name)
{
    if (!valid_name(name))
        return -1;
    int at = -1;
    if (find(name, &at) == 0)
        return 0;
    if (!take_ownership())
        return -1;
    for (int i = at; i < owned_count; i++)
        owned[i] = owned[i + 1];
    owned_count--;
    return 0;
}
