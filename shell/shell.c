// The Ceres shell (CeresASM plan/v2 F7): what `ceres run` starts when it is given no program. A prompt from which to
// move through the host directory and run programs:
//
//     ceres:/games> ls
//     ceres:/games> run snake.cres
//
// It is an ordinary program. `run` asks the machine to load the other program in its place (sys_run, command 3);
// when that program ends, `ceres run` starts the shell again with the program's environment and CERES_STATUS set to
// its exit status. So what the shell has to remember across a program - the directory it is in - it gives the
// program in PWD, and finds there when it comes back. The screen and the line history are the terminal's, and it
// keeps them from one program to the next.
//
// Built against the library into <build>/shell/shell.cres, and installed as shell/shell.cres of the directory Ceres
// is installed in (CERES_PATH), where `ceres run` finds it.
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "errno.h"
#include "signal.h"
#include "time.h"
#include "ceres/hostfs.h"
#include "ceres/sys.h"
#include "ceres/terminal.h"
#include "ceres/timer.h"
#include "ceres/video.h"

#define MAX_LINE   512
#define MAX_WORDS  32
#define MAX_PATH   256
#define MAX_ENV    64

// The directory the shell is in: a host path, "" for the root and "games/levels" below it (no slash at either end).
static char cwd[MAX_PATH];
static char** environment;

// ---- paths ----------------------------------------------------------------------------------------------------

// `name` taken from the current directory - or from the root when it starts with '/' - into out[MAX_PATH], with "."
// and ".." worked out (the host device takes neither). -1 when it is too long; ".." at the root stays there.
static int resolve(const char* name, char* out)
{
    char path[MAX_PATH * 2];
    if (name[0] == '/')
        path[0] = 0;
    else
        strcpy(path, cwd);
    size_t n = strlen(path);
    if (strlen(name) + n + 2 > sizeof path)
        return -1;
    if (n > 0)
        path[n++] = '/';
    strcpy(path + n, name);

    // Component by component into out.
    size_t len = 0;
    out[0] = 0;
    const char* p = path;
    while (*p)
    {
        while (*p == '/')
            p++;
        const char* start = p;
        while (*p && *p != '/')
            p++;
        size_t part = (size_t)(p - start);
        if (part == 0 || (part == 1 && start[0] == '.'))
            continue;
        if (part == 2 && start[0] == '.' && start[1] == '.')
        {
            char* slash = strrchr(out, '/');
            len = slash ? (size_t)(slash - out) : 0;
            out[len] = 0;
            continue;
        }
        if (len + part + 2 > MAX_PATH)
            return -1;
        if (len > 0)
            out[len++] = '/';
        memcpy(out + len, start, part);
        len += part;
        out[len] = 0;
    }
    return 0;
}

// What host_stat says of a path: 1 a directory, 0 a file, -1 nothing there (errno set).
static int kind_of(const char* path)
{
    if (path[0] == 0)
        return 1;                                // the root
    int r = host_stat(path);
    if (r == -EISDIR)
        return 1;
    if (r >= 0)
        return 0;
    errno = -r;
    return -1;
}

static void complain(const char* command, const char* what, const char* name)
{
    fprintf(stderr, "%s: %s: %s\n", command, what, name);
}

// Why a host operation failed, in words.
static const char* reason(int error)
{
    switch (error)
    {
        case ENOENT:       return "no such file or directory";
        case ENODEV:       return "no host directory (ceres run --host-dir)";
        case EISDIR:       return "is a directory";
        case ENOTDIR:      return "not a directory";
        case EINVAL:       return "not a valid path";
        case ENAMETOOLONG: return "path too long";
        case EACCES:       return "not allowed";
        default:           return "cannot be read";
    }
}

// ---- sizes and numbers ----------------------------------------------------------------------------------------

// A size in bytes, as "64 MiB", "12 KiB" or "300 bytes" - rounded down, so it does not change with a few bytes.
static void print_size(unsigned int bytes)
{
    if (bytes >= 1024u * 1024u)
        printf("%u MiB", bytes / (1024u * 1024u));
    else if (bytes >= 1024u)
        printf("%u KiB", bytes / 1024u);
    else
        printf("%u bytes", bytes);
}

static void print_hz(unsigned int hz)
{
    if (hz % 1000000u == 0)
        printf("%u MHz", hz / 1000000u);
    else if (hz % 1000u == 0)
        printf("%u kHz", hz / 1000u);
    else
        printf("%u Hz", hz);
}

// ---- the commands ---------------------------------------------------------------------------------------------

typedef int (*command_fn)(int argc, char** argv);

struct command
{
    const char* name;
    const char* usage;
    const char* what;
    command_fn run;
};

static int cmd_help(int argc, char** argv);
static int cmd_ls(int argc, char** argv);
static int cmd_cd(int argc, char** argv);
static int cmd_cat(int argc, char** argv);
static int cmd_run(int argc, char** argv);
static int cmd_clear(int argc, char** argv);
static int cmd_mem(int argc, char** argv);
static int cmd_time(int argc, char** argv);
static int cmd_info(int argc, char** argv);
static int cmd_reset(int argc, char** argv);
static int cmd_exit(int argc, char** argv);

static const struct command commands[] = {
    { "help",  "help [command]",     "what the commands do",                         cmd_help },
    { "ls",    "ls [dir]",           "the files and directories in a directory",     cmd_ls },
    { "cd",    "cd [dir]",           "go to a directory (/ alone: the top)",         cmd_cd },
    { "cat",   "cat <file>...",      "show what a file holds",                       cmd_cat },
    { "run",   "run <file> [args]",  "run a program (.cres), then come back here",   cmd_run },
    { "clear", "clear",              "clear the screen",                             cmd_clear },
    { "mem",   "mem",                "how much memory there is, and how much free",  cmd_mem },
    { "time",  "time",               "the date and time, and how long since start",  cmd_time },
    { "info",  "info",               "what this machine is",                         cmd_info },
    { "reset", "reset",              "start the machine again",                      cmd_reset },
    { "exit",  "exit [status]",      "leave the shell, and the machine stops",       cmd_exit },
    { 0, 0, 0, 0 },
};


static int cmd_help(int argc, char** argv)
{
    if (argc > 1)
    {
        for (const struct command* c = commands; c->name; c++)
            if (strcmp(c->name, argv[1]) == 0)
            {
                printf("%s\n  %s\n", c->usage, c->what);
                return 0;
            }
        complain("help", "no such command", argv[1]);
        return 1;
    }
    puts("Commands:");
    for (const struct command* c = commands; c->name; c++)
        printf("  %-18s %s\n", c->usage, c->what);
    puts("A program can also be run by its name alone: snake, or snake.cres.");
    puts("Up and Down bring back earlier lines.");
    return 0;
}

static int cmd_ls(int argc, char** argv)
{
    char dir[MAX_PATH];
    const char* name = argc > 1 ? argv[1] : ".";
    if (resolve(name, dir) != 0)
    {
        complain("ls", reason(ENAMETOOLONG), name);
        return 1;
    }
    int kind = kind_of(dir);
    if (kind < 0)
    {
        complain("ls", reason(errno), name);
        return 1;
    }
    if (kind == 0)
    {
        printf("%-24s %8d\n", name, host_stat(dir));
        return 0;
    }

    char entry[MAX_PATH];
    char full[MAX_PATH * 2];
    int count = 0;
    for (unsigned int i = 0;; i++)
    {
        int n = host_list(dir, i, entry, sizeof entry);
        if (n < 0)
        {
            complain("ls", reason(-n), name);
            return 1;
        }
        if (n == 0)
            break;
        count++;
        if (entry[n - 1] == '/')
            printf("%-24s    <dir>\n", entry);
        else
        {
            snprintf(full, sizeof full, "%s%s%s", dir, dir[0] ? "/" : "", entry);
            printf("%-24s %8d\n", entry, host_stat(full));
        }
    }
    if (count == 0)
        puts("(empty)");
    return 0;
}

static int cmd_cd(int argc, char** argv)
{
    char dir[MAX_PATH];
    const char* name = argc > 1 ? argv[1] : "/";
    if (resolve(name, dir) != 0)
    {
        complain("cd", reason(ENAMETOOLONG), name);
        return 1;
    }
    int kind = kind_of(dir);
    if (kind != 1)
    {
        complain("cd", kind == 0 ? "not a directory" : errno == ENOENT ? "no such directory" : reason(errno), name);
        return 1;
    }
    strcpy(cwd, dir);
    return 0;
}

static int cmd_cat(int argc, char** argv)
{
    if (argc < 2)
    {
        fputs("cat: which file? (cat <file>...)\n", stderr);
        return 1;
    }
    int status = 0;
    for (int i = 1; i < argc; i++)
    {
        char path[MAX_PATH];
        if (resolve(argv[i], path) != 0)
        {
            complain("cat", reason(ENAMETOOLONG), argv[i]);
            status = 1;
            continue;
        }
        int handle = path[0] ? host_open(path, HOST_READ) : -EISDIR;
        if (handle < 0)
        {
            complain("cat", reason(-handle), argv[i]);
            status = 1;
            continue;
        }
        char buf[256];
        int n;
        while ((n = host_read(handle, buf, sizeof buf)) > 0)
            fwrite(buf, 1, (size_t)n, stdout);
        host_close(handle);
    }
    return status;
}

// The environment a program is started with: the shell's, with PWD saying where it is (and no CERES_STATUS, which is
// what the machine adds when the program ends).
static char** child_environment(void)
{
    static char* list[MAX_ENV + 2];
    static char pwd[MAX_PATH + 8];
    int n = 0;
    for (char** e = environment; e && *e && n < MAX_ENV; e++)
        if (strncmp(*e, "PWD=", 4) != 0 && strncmp(*e, "CERES_STATUS=", 13) != 0)
            list[n++] = *e;
    snprintf(pwd, sizeof pwd, "PWD=/%s", cwd);
    list[n++] = pwd;
    list[n] = 0;
    return list;
}

static int cmd_run(int argc, char** argv)
{
    if (argc < 2)
    {
        fputs("run: which program? (run <file.cres> [arguments])\n", stderr);
        return 1;
    }
    static char path[MAX_PATH + 8];
    if (resolve(argv[1], path) != 0)
    {
        complain("run", reason(ENAMETOOLONG), argv[1]);
        return 1;
    }
    int kind = kind_of(path);
    size_t n = strlen(path);
    if (kind < 0 && errno == ENOENT && (n < 5 || strcmp(path + n - 5, ".cres") != 0))
    {
        strcat(path, ".cres");                   // snake: snake.cres
        kind = kind_of(path);
    }
    if (kind != 0)
    {
        complain("run", kind == 1 ? "is a directory" : errno == ENOENT ? "no such program" : reason(errno), argv[1]);
        return 1;
    }

    // argv[0] is the program's path; the rest are the words after its name.
    static char* child[MAX_WORDS + 1];
    int count = 0;
    child[count++] = path;
    for (int i = 2; i < argc; i++)
        child[count++] = argv[i];
    child[count] = 0;
    sys_run(path, count, child, child_environment());
    complain("run", "not a program this machine can load", argv[1]);
    return 1;
}

static int cmd_clear(int argc, char** argv)
{
    fputs("\x1b[2J\x1b[H", stdout);
    return 0;
}

static int cmd_mem(int argc, char** argv)
{
    fputs("RAM   ", stdout);
    print_size(sys_memory_size());
    fputs(", ", stdout);
    print_size(sys_stack_free());
    puts(" free");
    fputs("VRAM  ", stdout);
    print_size(sys_vram_size());
    putchar('\n');
    return 0;
}

static int cmd_time(int argc, char** argv)
{
    time_t now = time(0);
    struct tm tm;
    gmtime_r(&now, &tm);
    char text[32];
    strftime(text, sizeof text, "%Y-%m-%d %H:%M:%S", &tm);
    unsigned int up = timer_millis() / 1000u;
    printf("%s UTC, up %u:%02u:%02u\n", text, up / 3600u, up / 60u % 60u, up % 60u);
    return 0;
}

static int cmd_info(int argc, char** argv)
{
    static const char* const profiles[] = { "micro", "pocket", "retro", "arcade", "polygon", "standard", "workstation", "custom" };
    unsigned int profile = sys_profile();
    printf("machine   %s, CPU ", profile < 8 ? profiles[profile] : "unknown");
    print_hz(timer_cpu_hz());
    fputs("\nmemory    ", stdout);
    print_size(sys_memory_size());
    fputs(" of RAM, ", stdout);
    print_size(sys_vram_size());
    puts(" of VRAM");
    printf("video     V%d, up to V%d; %dx%d at %d Hz\n", video_level(), video_max_level(), video_width(), video_height(), video_refresh());
    printf("terminal  %dx%d\n", term_cols(), term_rows());
    printf("host      %s\n", host_available() ? "a directory is attached" : "no directory (ceres run --host-dir)");
    return 0;
}

static int cmd_reset(int argc, char** argv)
{
    sys_reset();
}

static int cmd_exit(int argc, char** argv)
{
    exit(argc > 1 ? atoi(argv[1]) : 0);
}


// ---- the line -------------------------------------------------------------------------------------------------

// Splits a line into words at spaces and tabs; "double quotes" keep spaces in one. The words point into the line.
static int split(char* line, char** words)
{
    int count = 0;
    char* p = line;
    while (*p)
    {
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == 0)
            break;
        if (count == MAX_WORDS)
            return -1;
        char* out = p;
        words[count++] = out;
        int quoted = 0;
        while (*p && (quoted || (*p != ' ' && *p != '\t')))
        {
            if (*p == '"')
                quoted = !quoted;
            else
                *out++ = *p;
            p++;
        }
        if (*p)
            p++;
        *out = 0;
    }
    return count;
}

static void prompt(void)
{
    printf("ceres:/%s> ", cwd);
    fflush(stdout);
}

// Ctrl+C at the prompt drops the line (the terminal does that) and gives a fresh prompt.
static void on_interrupt(int signal)
{
    putchar('\n');
    prompt();
}

static int execute(char* line)
{
    char* words[MAX_WORDS];
    int count = split(line, words);
    if (count < 0)
    {
        fprintf(stderr, "too many words: at most %d\n", MAX_WORDS);
        return 1;
    }
    if (count == 0)
        return 0;
    for (const struct command* c = commands; c->name; c++)
        if (strcmp(c->name, words[0]) == 0)
            return c->run(count, words);

    // Not a command: a program by its name, if there is one.
    char path[MAX_PATH + 8];
    if (resolve(words[0], path) == 0)
    {
        int kind = kind_of(path);
        if (kind < 0)
        {
            strcat(path, ".cres");
            kind = kind_of(path);
        }
        if (kind == 0)
        {
            char* run[MAX_WORDS + 1];
            run[0] = "run";
            for (int i = 0; i < count; i++)
                run[i + 1] = words[i];
            return cmd_run(count + 1, run);
        }
    }
    fprintf(stderr, "unknown command: %s ('help' lists them)\n", words[0]);
    return 1;
}

int main(int argc, char** argv, char** envp)
{
    environment = envp;
    signal(SIGINT, on_interrupt);

    // Where it was: PWD, when the shell comes back from a program, and still a directory.
    const char* pwd = getenv("PWD");
    char where[MAX_PATH];
    if (pwd && pwd[0] == '/' && resolve(pwd, where) == 0 && kind_of(where) == 1)
        strcpy(cwd, where);

    // A clear screen - the machine has just started, or was reset - gets the greeting; after a program, the shell
    // says how it ended when that was not with 0.
    int x, y;
    term_cursor(&x, &y);
    const char* status = getenv("CERES_STATUS");
    if (x == 0 && y == 0)
        puts("Ceres shell. Type 'help' for the commands.");
    else if (status && strcmp(status, "0") != 0)
        printf("(exit status %s)\n", status);

    char line[MAX_LINE];
    for (;;)
    {
        // A Ctrl+C pressed while a command ran has done its part (there is nothing to interrupt now): it goes, so
        // the read does not answer it with a second prompt.
        if (term_interrupted())
        {
            mmio_w32(TERM_INT_ACK, 1u);
            putchar('\n');
        }
        prompt();
        if (!fgets(line, sizeof line, stdin))
        {
            putchar('\n');                       // the input ended (Ctrl+D): so does the shell
            return 0;
        }
        size_t n = strlen(line);
        if (n > 0 && line[n - 1] == '\n')
            line[n - 1] = 0;
        execute(line);
    }
}
