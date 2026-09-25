# Ceres Standard Library

The C library of the [Ceres](../../CeresASM) virtual machine, compiled by [Ceres-C](../../Ceres-C): the C standard
library a program expects - `stdio`, `stdlib`, `string`, `math`, `time`, `setjmp`, `locale`, `wchar`, `uchar` and the
rest - and, under `ceres/`, everything the machine has to offer a program: its devices (terminal, keyboard, mouse,
gamepad, text and pixel display, blitter, audio, disk, peripheral ports, timer, MMU, interrupts), a file system,
graphics, sprites, fonts, music, tasks and channels, and the pieces every program ends up writing (containers,
arenas, hashing, JSON, INI, saved games, images, compression, resource packs).

Every header documents itself; [docs/reference](docs/reference/README.md) has them all as pages, generated from the
headers by `node tools/gendocs.js`.

## Building

On any system, with CMake 3.21+ (and Ninja, when it is there), GNU Make 4.2+ and Node 16+ for the tests:

```sh
make                                        # the library at -O2: build/cmake/O2/libceres.car, libceres.decls.casm, obj/
make OPT=s                                  # at -Os (OPT: 0, 1, 2, 3, s, g), in build/cmake/Os
make -j levels LEVELS="0 1 2 s"             # several levels at once, one directory each
make SOFT_DOUBLE=1                          # for -fsoft-double programs, in build/cmake/O2-sd
make FS_MAX_OPEN=4 CERES_HEAP_DEBUG=1       # any setting of include/ceres/config.h
make FLAGS="-fno-inline" DEFINES="NDEBUG"   # optimizations one by one, more macros
make sysroot PREFIX=<dir>                   # <dir>/include, <dir>/lib and <dir>/lib/soft-double
make help                                   # everything else
```

The build is `CMakeLists.txt`, and the Makefile only drives it; CMake works on its own too, with any generator:

```sh
cmake --preset O2 && cmake --build --preset O2 && ctest --preset O2      # CMakePresets.json: O0 ... Og, O2-sd, ...
cmake -S . -B build/cmake/mine -G Ninja -DCERES_OPT_LEVEL=s -DFS_MAX_OPEN=4 -DCERES_SOFT_DOUBLE=ON
cmake --build build/cmake/mine -j && cmake --install build/cmake/mine --prefix <dir>
```

Every C file is compiled on its own and assembled against the library's merged declarations, so the build is
parallel and incremental: a changed source rebuilds its unit, a changed header the units that include it. Its
cache variables are the settings: `CERES_OPT_LEVEL`, `CERES_SOFT_DOUBLE`, `CERES_WERROR`, `CERES_OPT_FLAGS`,
`CERES_DEFINES`, `CERES_EXTRA_FLAGS`, `CERES_OPTIONAL_MODULES`, `CERES_LIBDIR`, and one for every setting of
`include/ceres/config.h` (read from the header, so a new one there is one here). `<build>/libceres.flags` records
the flags a build was compiled with.

`ceresc` and `ceres` are found next to this checkout (`../../Ceres-C`, `../../CeresASM`), through the `CERESC`
and `CERES_PATH` environment variables, or on `PATH`; `make CERESC=... CERES=...` (or `-DCERESC=`, `-DCERES=`)
names them outright.

The PowerShell scripts of before still work on Windows: `tools/mklib.ps1` (build/lib/O<n>, `-SoftDouble`) and
`tools/install.ps1 -Prefix <dir>`.

## The Makefile, target by target

### One rule first

Every `make` configures the build again with **exactly** the variables of that command; what it does not say
goes back to its default. So

```sh
make FS_MAX_OPEN=4
make install PREFIX=/opt/ceres       # builds again with the default FS_MAX_OPEN, and installs THAT
```

A configuration other than the default is used by saying the same variables to every command - or by writing
them once in `config.mk` at the top of the checkout (ignored by git; `CONFIG_FILE=<file>` names another), which
every command reads. The command line still wins over it:

```make
# config.mk
OPT = s
FS_MAX_OPEN = 4
PREFIX = /opt/ceres
```

The build directory follows from two variables: `build/cmake/O<OPT>`, with `-sd` when `SOFT_DOUBLE=1`
(`make OPT=s` builds in `build/cmake/Os`). Each level keeps its own; two configurations at one level share one, and
the later replaces the earlier.

### The variables

What to build:

| Variable | Default | What it does |
| --- | --- | --- |
| `OPT` | `2` | The optimization level: `0`, `1`, `2`, `3` (the same as 2), `s` (size), `g` (debugging). |
| `LEVELS` | `0 1 2` | The levels of `make levels` and `make test`. |
| `SOFT_DOUBLE` | `0` | `1` (or `yes`, `on`, `true`): the library for programs compiled with `-fsoft-double`. |
| `WERROR` | `1` | `0`: warnings are not errors. |
| `FLAGS` | | Optimizations one by one, on top of the level: `FLAGS="-fno-inline -fcse"`. `ceresc --help` lists them; anything not shaped `-f...`/`-fno-...` is refused. |
| `DEFINES` | | More macros for every unit: `DEFINES="NDEBUG TRACE=2"`. |
| `CERESC_FLAGS` | | Anything else for ceresc, separated by blanks. |
| `MODULES` | `irq fault mmu` | The optional modules `install` puts beside the archive as objects of their own (`-lceres_irq`...). The archive holds all of them either way. |

The settings of `include/ceres/config.h` - numbers, empty for the header's default. They are taken from the
command line or `config.mk`, never from the environment:

| Variable | Default | What it sets |
| --- | --- | --- |
| `CERES_ATEXIT_SLOTS` | 32 | How many functions `atexit()` holds. |
| `CERES_HEAP_STACK_RESERVE` | 16384 | The bytes `malloc` keeps free between the heap and the stack. |
| `CERES_HEAP_DEBUG` | 0 | `1`: malloc checks itself as it goes (slower, 8 bytes more a block). |
| `FS_MAX_OPEN` | 8 | How many CeresFS files are open at once. |
| `TASK_MAX` | 16 | How many `ceres/task.h` tasks exist at once. |
| `TASK_STACK_DEFAULT` | 8192 | The stack of a task that asks for 0. |
| `TIMER_MAX_TASKS` | 8 | How many `timer_after`/`timer_every` timers exist at once. |

Where, and with what:

| Variable | Default | What it does |
| --- | --- | --- |
| `BUILD_ROOT` | `build/cmake` | Where the build directories go. |
| `BUILD_DIR` | `$(BUILD_ROOT)/O<OPT>[-sd]` | A build directory of your own naming (`BUILD_DIR=build/mine`). |
| `GENERATOR` | `Ninja`, when it is there | Any CMake generator (`"Unix Makefiles"`, `"MinGW Makefiles"`...). To change it in a directory that exists, `make clean` first. |
| `JOBS` | | Parallel jobs for `cmake --build` and `ctest` (Ninja is parallel already). |
| `PREFIX` | | Where `install` and `sysroot` put the library. **Required** by those two. |
| `CERESC`, `CERES` | found | The tools. Without them: the `CERESC` and `CERES_PATH` environment variables, the sibling checkouts, then `PATH`. **Once given, a build directory remembers them**: give the other one, or `make clean`, to go back. |
| `CMAKE`, `CTEST`, `NODE` | `cmake`, `ctest`, `node` | For tools that are not on `PATH`. |
| `CONFIG_FILE` | `config.mk` | Another configuration file. |

Tests: `TEST="a b"` (only those), `GC=1` (link them with `--gc-sections`), `TIMEOUT=900` (seconds a program may
take; 180 when not said).

### The targets

**`make`, `make lib`** - configures and builds the library in `BUILD_DIR`: `libceres.car` (the archive),
`libceres.decls.casm` (its declarations), `obj/` (an object per unit) and `libceres.flags` (what it was compiled
with). Incremental: with nothing changed it does nothing. Takes every build variable and the config.h settings,
`BUILD_DIR`, `GENERATOR`, `JOBS`, `CERESC`, `CERES`.

```sh
make OPT=s FS_MAX_OPEN=4 FLAGS="-fno-inline"
ceresc prog.c build/cmake/Os/libceres.car --decls build/cmake/Os/libceres.decls.casm -I include -Os --run
```

**`make configure`** - only the configure step: to see whether a configuration is valid (`OPT=5` and
`FS_MAX_OPEN=abc` are refused here) or to open the directory in an IDE. The same variables as `lib`, but `JOBS`.

**`make levels`** - one library per level in `LEVELS`, each in `$(BUILD_ROOT)/O<level>` (`-sd` with
`SOFT_DOUBLE=1`); with `-j`, all at once. Every build variable applies to every level; `BUILD_DIR` is ignored,
since each level needs its own. `make lib-O<level>` builds one level without touching `OPT` (`make lib-Os`).

```sh
make -j levels LEVELS="0 2 s"
```

**`make soft-double`** - `make SOFT_DOUBLE=1`: the library for `-fsoft-double` programs, in
`build/cmake/O<OPT>-sd`. Give it a `BUILD_DIR` only to send it there.

**`make install`** - builds, then installs the layout `ceresc --sysroot` expects:
`PREFIX/include/` (every header), `PREFIX/lib/libceres.car` and `libceres.decls.casm` (`lib/soft-double/` for a
soft-double build), and `PREFIX/lib/libceres_<module>.cobj` for the modules in `MODULES`. A program then needs
`ceresc prog.c --sysroot /opt/ceres -lceres --run` and nothing else. `PREFIX` is required, and the build variables
must be the ones the library was built with (or `config.mk`), or another configuration is what gets installed. It
removes nothing an earlier install left: a module taken out of `MODULES` stays until you delete it.

**`make sysroot`** - installs both kinds into one `PREFIX`: the library in `lib/`, the soft-double one in
`lib/soft-double/`, so `ceresc -lceres` picks the right one by whether the program uses `-fsoft-double`. `PREFIX` is
required; `OPT` and every other setting apply to both; `BUILD_DIR` is ignored.

```sh
make sysroot PREFIX=/opt/ceres OPT=2
```

**`make check`** - builds, then runs `ctest` on that build:

- `verify.hello`, `verify.test_user_irq17` (and `verify.test_double` with `SOFT_DOUBLE=1`): programs linked against
  the archive with ceresc alone, run, and compared with what they must print;
- `suite`: every test of `tests/` against this library, at its level - when Node is there and the build is not
  soft double.

The build variables say which build is checked; `JOBS` runs the tests in parallel. It is how one configuration is
checked, the soft-double one included. A configuration that is not the default can change what tests print or how
long they take: with `CERES_HEAP_DEBUG=1`, `test_disk_fs` takes longer than the 180 seconds a program gets
(`make test TIMEOUT=900` gives it more).

**`make test`** - the whole suite at every level of `LEVELS`, against libraries built here: it builds
`$(BUILD_ROOT)/O<level>` for each level, and `O2` for the examples, then runs every test, compiles each header on
its own and runs the examples. Takes `LEVELS`, `TEST`, `GC`, `TIMEOUT` - and the build variables, so
`make test FS_MAX_OPEN=3` tests that configuration. `BUILD_DIR` is ignored (`BUILD_ROOT` is not). Refused with
`SOFT_DOUBLE=1`, before anything is built: what the tests must print was written for the library without soft
double - `make check SOFT_DOUBLE=1` checks that one.

```sh
make test LEVELS="0 2" GC=1
```

**`make test-NAME`** - the same for `tests/NAME.c` alone, at every level of `LEVELS` (the headers and examples are
checked in the same run): `make test-test_json LEVELS=2`.

**`make update-NAME`** - runs the test at `-O0` and **rewrites** `tests/expected/NAME.expected` with what it
prints: for when a change to the library changes a test's output for a good reason. Review it with `git diff`
before committing. Builds `O0` and `O2`; `LEVELS`, `TEST`, `GC` and `TIMEOUT` do not apply.

**`make headers`** - compiles every header on its own, to prove each is self-contained. It uses no build: only
`NODE`, and `CERESC` when given.

**`make docs`** - writes `docs/reference/` again from the headers' comments; run it after changing a header, and
commit what it writes. Only `NODE`.

**`make config`** - prints what the variables come to (the directory, the generator, the level, the options and
every config.h setting) and builds nothing: `make config OPT=s FS_MAX_OPEN=4` shows what that command would build.

**`make help`** - all of the above, in short.

**`make clean`** - removes only the current configuration's directory (`make clean OPT=s` removes
`build/cmake/Os`). **`make distclean`** removes all of `build/`: every CMake build, those of `mklib.ps1` and
`runtests.js`, and what the tests leave behind.

### Common workflows

```sh
make -j test                                   # build and test the default library
make OPT=g BUILD_DIR=build/debug DEFINES=TRACE # a debugging build of its own, beside the default ones
```

A configuration of your own, checked and installed as a sysroot - `config.mk`:

```make
OPT = s
FS_MAX_OPEN = 4
CERES_HEAP_DEBUG = 1
PREFIX = /opt/ceres
```

then `make check` and `make sysroot`. The same settings are CMake cache variables for a build without make
(`cmake -S . -B <dir> -DCERES_OPT_LEVEL=s -DFS_MAX_OPEN=4`), and `CMakePresets.json` has ready ones
(`cmake --preset Os`, `cmake --build --preset Os`, `ctest --preset Os`).

## Using it

With a sysroot installed, a program needs nothing else:

```sh
ceresc prog.c --sysroot <dir> -lceres -O2 --run
ceresc prog.c --sysroot <dir> -lceres -O2 --gc-sections --run      # leave out the functions nothing reaches
ceresc prog.c --sysroot <dir> -lceres -O2 -fsoft-double --run      # double as a real 64-bit IEEE double
```

Without one, name the archive and its declarations:
`ceresc prog.c build/lib/O2/libceres.car --decls build/lib/O2/libceres.decls.casm -I include -O2 --run`.

Three modules bind interrupt vectors, and a program gets them only by naming them, since a vector can be bound
once in a program: `-lceres_irq` (`ceres/irq.h`), `-lceres_fault -lceres_fault_asm` (fault reports) and
`-lceres_mmu -lceres_mmu_asm` (page faults, `ceres/mmu.h`).

A few things this library is that a desktop libc is not:

- **One floating-point format.** The machine has IEEE binary32, and `double` is `float` unless a program is compiled
  with `-fsoft-double`; then `double` is a real binary64 computed in software (`ceres/f64.h`), and a library built
  with `SOFT_DOUBLE=1` prints and reads it whole. The math functions are float either way.
- **Exact conversions.** `printf`, `scanf`, `strtof`/`strtod` convert between binary and decimal exactly, correctly
  rounded, whatever the number of digits (`src/fconv.c`, `src/fconv64.c`).
- **UTF-8.** Strings, the multibyte functions (`MB_CUR_MAX` is 4), `%lc`/`%ls`, the text framebuffer and the font
  (Latin-1 glyphs) all speak it (`ceres/utf8.h`).
- **No threads, cooperative tasks.** `ceres/task.h` has tasks and channels that switch only where they wait.

## Testing

```sh
make test                               # every test at each level of LEVELS (0 1 2), against the libraries built here
make test-test_json LEVELS=2            # one test
make update-test_json                   # rewrite its tests/expected/test_json.expected from a -O0 run
make check                              # ctest in the build directory: programs linked against it, and the suite
node tools/runtests.js                  # the runner alone: it builds build/lib/O<n> itself (or --library <dir>)
powershell tools/runtests.ps1           # the same runner in PowerShell (-Test, -Levels, -Update, -GcSections)
```

`make test` runs the suite against the library as configured (`make test FS_MAX_OPEN=3` tests that one); what
the tests must print was written for the defaults, without soft double.

A test is `tests/<name>.c`; what it must print is `tests/expected/<name>.expected`, byte for byte. Beside it can be
`.stderr` (its error stream), `.status` (its exit status), `.stdin` (what it reads), `.flags` (compiler options -
the library is then compiled with them too), `.run` (more for `ceres run`: `--env`, `--host-dir build/host`,
`-- arguments`) and `.ports` (sticks and cartridges to plug in). A `// USE: irq` line near the top links an
optional module.

## Tools

| Script | What it does |
| --- | --- |
| `Makefile`, `CMakeLists.txt`, `CMakePresets.json` | The build: `make help`. |
| `tools/cmake/*.cmake` | The build's steps: header dependencies, merging the declarations, the verify tests. |
| `tools/mklib.ps1` | Builds the library archives per optimization level, in PowerShell. |
| `tools/install.ps1` | Lays out a sysroot for `ceresc --sysroot`, in PowerShell. |
| `tools/runtests.js`, `tools/runtests.ps1` | The test runner. |
| `tools/gendocs.js` | Writes `docs/reference` from the headers. |
| `tools/mkpack.js` | Builds a resource pack (`ceres/pack.h`) from host files, as a cartridge image. |
| `tools/gen_font.js` | Generates the 8x8 font (`src/ceres/font_data.inc`), and the VM's text-window font. |
| `tools/gen_tables.js` | The constant tables the sources include (the sine of `ceres/fixed.h`, the CRC-32 table). |
| `tools/gen_math_tables.js`, `tools/gen_f64_vectors.js` | Reference values for the math and soft-double tests. |
| `tools/example.ps1` | Builds one program from `examples/` and runs it. |
| `tools/consolecheck.ps1` | Checks the text interface against a real Windows console. |
