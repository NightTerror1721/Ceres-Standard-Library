# Ceres Standard Library

The C library of the [Ceres](../../CeresASM) virtual machine, compiled by [Ceres-C](../../Ceres-C): the C standard
library a program expects - `stdio`, `stdlib`, `string`, `math`, `time`, `setjmp`, `locale`, `wchar`, `uchar` and the
rest - and, under `ceres/`, everything the machine has to offer a program: its devices (terminal, keyboard, mouse,
gamepad, GPU (text plane, bitmap, copy engine), blitter, audio, disk, peripheral ports, timer, MMU, interrupts), a file system,
graphics, sprites, fonts, music, tasks and channels, and the pieces every program ends up writing (containers,
arenas, hashing, JSON, INI, saved games, images, compression, resource packs). And the **Ceres shell**
(`shell/shell.c`), the prompt `ceres run` starts when it is given no program.

Every header documents itself; [docs/reference](docs/reference/README.md) has them all as pages, generated from the
headers by `node tools/gendocs.js`.

## Building

On any system, with CMake 3.21+ (and Ninja, when it is there), GNU Make 4.2+ and Node 16+ for the tests:

```sh
make                                        # the library at -O2: build/cmake/O2/libceres.car, libceres.decls.casm, obj/
make OPT=s                                  # at -Os (OPT: 0, 1, 2, 3, s, g), in build/cmake/Os
make -j levels LEVELS="0 1 2 s"             # several levels at once, one directory each
make FS_MAX_OPEN=4 CERES_HEAP_DEBUG=1       # any setting of include/ceres/config.h
make FLAGS="-fno-inline" DEFINES="NDEBUG"   # optimizations one by one, more macros
make help                                   # everything else
```

The build is `CMakeLists.txt`, and the Makefile only drives it; CMake works on its own too, with any generator:

```sh
cmake --preset O2 && cmake --build --preset O2 && ctest --preset O2      # CMakePresets.json: O0 ... Og
cmake -S . -B build/cmake/mine -G Ninja -DCERES_OPT_LEVEL=s -DFS_MAX_OPEN=4
cmake --build build/cmake/mine -j
```

Every C file is compiled on its own and assembled against the library's merged declarations, so the build is
parallel and incremental: a changed source rebuilds its unit, a changed header the units that include it. Its
cache variables are the settings: `CERES_OPT_LEVEL`, `CERES_WERROR`, `CERES_OPT_FLAGS`,
`CERES_DEFINES`, `CERES_EXTRA_FLAGS`, `CERES_OPTIONAL_MODULES`, and one for every setting of
`include/ceres/config.h` (read from the header, so a new one there is one here). `<build>/libceres.flags` records
the flags a build was compiled with.

A build also leaves, in its directory, what goes in the directory Ceres is installed in (`CERES_PATH`), laid out as
it goes there: `stdlib/include` (the headers), `stdlib/lib` (`libceres.car`, `libceres.decls.casm` and the optional
modules as `libceres_<module>.cobj`) and `shell/` (`shell.cres` and `shell-small.cres`). There is nothing to install from here: the
[Ceres Binaries](../../CeresBinaries) project builds `ceres`, `ceresc` and this library together and packages them,
with an installer for each system.

`ceresc` and `ceres` are found the way `ceresc` finds `ceres`: `make CERESC=... CERES=...` (or `-DCERESC=`,
`-DCERES=`, or the `CERESC` and `CERES` environment variables: the executable, or the directory that holds it)
names them outright; without that, they are the ones in the directory the `CERES_PATH` environment variable names
(where Ceres is installed), and then the ones on `PATH`. A variable that is set decides - one that names no such
tool is an error, not a reason to look elsewhere - and nothing is looked for next to this checkout. The scripts and
the test runners find them the same way.

The PowerShell scripts of before still work on Windows: `tools/mklib.ps1` (build/lib/O<n>).

## The Makefile, target by target

### One rule first

Every `make` configures the build again with **exactly** the variables of that command; what it does not say
goes back to its default. So

```sh
make FS_MAX_OPEN=4
make check                           # builds again with the default FS_MAX_OPEN, and checks THAT
```

A configuration other than the default is used by saying the same variables to every command - or by writing
them once in `config.mk` at the top of the checkout (ignored by git; `CONFIG_FILE=<file>` names another), which
every command reads. The command line still wins over it:

```make
# config.mk
OPT = s
FS_MAX_OPEN = 4
```

The build directory follows from the level: `build/cmake/O<OPT>` (`make OPT=s` builds in `build/cmake/Os`). Each level keeps its own; two configurations at one level share one, and
the later replaces the earlier.

### The variables

What to build:

| Variable | Default | What it does |
| --- | --- | --- |
| `OPT` | `2` | The optimization level: `0`, `1`, `2`, `3` (the same as 2), `s` (size), `g` (debugging). |
| `LEVELS` | `0 1 2` | The levels of `make levels` and `make test`. |
| `WERROR` | `1` | `0`: warnings are not errors. |
| `FLAGS` | | Optimizations one by one, on top of the level: `FLAGS="-fno-inline -fcse"`. `ceresc --help` lists them; anything not shaped `-f...`/`-fno-...` is refused. |
| `DEFINES` | | More macros for every unit: `DEFINES="NDEBUG TRACE=2"`. |
| `CERESC_FLAGS` | | Anything else for ceresc, separated by blanks. |
| `MODULES` | `irq fault mmu` | The optional modules put in `stdlib/lib` beside the archive as objects of their own (`-lceres_irq`...). The archive holds all of them either way. |

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
| `BUILD_DIR` | `$(BUILD_ROOT)/O<OPT>` | A build directory of your own naming (`BUILD_DIR=build/mine`). |
| `GENERATOR` | `Ninja`, when it is there | Any CMake generator (`"Unix Makefiles"`, `"MinGW Makefiles"`...). To change it in a directory that exists, `make clean` first. |
| `JOBS` | | Parallel jobs for `cmake --build` and `ctest` (Ninja is parallel already). |
| `CERESC`, `CERES` | found | The tools. Without them: the directory `CERES_PATH` names, then `PATH`, looked up again on every configure. |
| `CMAKE`, `CTEST`, `NODE` | `cmake`, `ctest`, `node` | For tools that are not on `PATH`. |
| `CONFIG_FILE` | `config.mk` | Another configuration file. |

Tests: `TEST="a b"` (only those), `GC=1` (link them with `--gc-sections`), `TIMEOUT=900` (seconds a program may
take; 180 when not said).

### The targets

**`make`, `make lib`** - configures and builds the library in `BUILD_DIR`: `libceres.car` (the archive),
`libceres.decls.casm` (its declarations), `obj/` (an object per unit) and `libceres.flags` (what it was compiled
with), and `stdlib/` and `shell/` as they go where Ceres is installed. Incremental: with nothing changed it does nothing. Takes every build variable and the config.h settings,
`BUILD_DIR`, `GENERATOR`, `JOBS`, `CERESC`, `CERES`.

```sh
make OPT=s FS_MAX_OPEN=4 FLAGS="-fno-inline"
ceresc prog.c build/cmake/Os/libceres.car --decls build/cmake/Os/libceres.decls.casm -I include -Os --run
```

**`make configure`** - only the configure step: to see whether a configuration is valid (`OPT=5` and
`FS_MAX_OPEN=abc` are refused here) or to open the directory in an IDE. The same variables as `lib`, but `JOBS`.

**`make levels`** - one library per level in `LEVELS`, each in `$(BUILD_ROOT)/O<level>`; with `-j`, all at once. Every build variable applies to every level; `BUILD_DIR` is ignored,
since each level needs its own. `make lib-O<level>` builds one level without touching `OPT` (`make lib-Os`).

```sh
make -j levels LEVELS="0 2 s"
```

**`make check`** - builds, then runs `ctest` on that build:

- `verify.hello`, `verify.test_user_irq19` and `verify.test_double`: programs linked against the archive with ceresc
  alone, run, and compared with what they must print;
- `suite`: every test of `tests/` against this library, at its level - when Node is there.

The build variables say which build is checked; `JOBS` runs the tests in parallel. It is how one configuration is
checked. A configuration that is not the default can change what tests print or how
long they take: with `CERES_HEAP_DEBUG=1`, `test_disk_fs` takes longer than the 180 seconds a program gets
(`make test TIMEOUT=900` gives it more).

**`make test`** - the whole suite at every level of `LEVELS`, against libraries built here: it builds
`$(BUILD_ROOT)/O<level>` for each level, and `O2` for the examples, then runs every test, compiles each header on
its own and runs the examples. Takes `LEVELS`, `TEST`, `GC`, `TIMEOUT` - and the build variables, so
`make test FS_MAX_OPEN=3` tests that configuration. `BUILD_DIR` is ignored (`BUILD_ROOT` is not).

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

A configuration of your own, checked - `config.mk`:

```make
OPT = s
FS_MAX_OPEN = 4
CERES_HEAP_DEBUG = 1
```

then `make check`. The same settings are CMake cache variables for a build without make
(`cmake -S . -B <dir> -DCERES_OPT_LEVEL=s -DFS_MAX_OPEN=4`), and `CMakePresets.json` has ready ones
(`cmake --preset Os`, `cmake --build --preset Os`, `ctest --preset Os`).

## Using it

With Ceres installed (the directory `CERES_PATH` names, or the one `ceresc` is in), `--stdlib` is all a program
needs: the library's headers join the include search, and the archive is linked.

```sh
ceresc prog.c --stdlib -O2 --run
ceresc prog.c --stdlib -O2 --gc-sections --run      # leave out the functions nothing reaches
ceresc prog.c --stdlib -O2 -fshort-double --run     # double as float: the float math under the standard names
```

Against a library built here, name the archive and its declarations:
`ceresc prog.c build/cmake/O2/libceres.car --decls build/cmake/O2/libceres.decls.casm -I include -O2 --run`.

Three modules bind interrupt vectors, and a program gets them only by naming them, since a vector can be bound
once in a program: `-lceres_irq` (`ceres/irq.h`), `-lceres_fault -lceres_fault_asm` (fault reports) and
`-lceres_mmu -lceres_mmu_asm` (page faults, `ceres/mmu.h`), found in `stdlib/lib` with `--stdlib`.

A few things this library is that a desktop libc is not:

- **Two floating-point formats, both native.** `float` is IEEE binary32 and `double` (and `long double`, the same
  type) IEEE binary64, each computed by the machine's own instructions - the doubles on register pairs. `<math.h>`
  has both families: `sin`, `pow`, `erf`... take and give a `double`, within about an ulp for most of them (the
  header lists how close each one is), and `sinf`, `powf`... are the faster `float` ones. A program compiled with
  `-fshort-double` makes `double` a `float` and links the same library: its headers give it the `float` family
  under the standard names, and `strtod`, `atof`, `difftime` and `scanf`'s `%lf` in `float`; `printf` needs
  nothing, since a `float` passed through `...` travels as a binary64 either way.
- **Exact conversions.** `printf`, `scanf`, `strtof`/`strtod` convert between binary and decimal exactly, correctly
  rounded, whatever the number of digits (`src/fconv.c`, `src/fconv64.c`).
- **UTF-8.** Strings, the multibyte functions (`MB_CUR_MAX` is 4), `%lc`/`%ls`, the terminal, the text plane and the
  font (Latin-1 glyphs and box drawing) all speak it (`ceres/utf8.h`).
- **No threads, cooperative tasks.** `ceres/task.h` has tasks and channels that switch only where they wait.

## The shell

`shell/shell.c` is the prompt of the machine: `ceres run` without a program starts `shell/shell.cres` of the
directory Ceres is installed in - the one `CERES_PATH` names, or else the one `ceres` is in (CeresASM
[docs/36](../../CeresASM/docs/36-Shell-and-Program-Loading.md)). A build leaves it in `<build>/shell/shell.cres`, and
again as `<build>/shell/shell-small.cres`: the same source built with `SHELL_SMALL`, which formats what it prints
with a small `printf` of its own instead of the library's (whose floating point is most of the whole shell). The
whole shell needs about 90 KB of RAM and the small one about 53 KB; `ceres run` starts the first that fits the
machine, so `micro`, with 64 KiB, gets the small one and still has a shell. Both print the same: the runners type
every session of `tests/shell` on each, and `tests/shell/micro` runs on a micro machine.

```sh
ceres run                                  # the host directory is the current one; --host-dir names another
```

```
Ceres shell. Type 'help' for the commands.
ceres:/> cd games
ceres:/games> ls
snake.cres                  76148
ceres:/games> snake
```

Its commands: `help`, `ls`, `cd`, `cat`, `run` (or a program by its name alone), `clear`, `mem`, `time`, `info`,
`reset` and `exit`; Up and Down bring back earlier lines. It is an ordinary program built against the library, and
any program can do what its `run` does: `sys_run(path, argc, argv, envp)` (`ceres/sys.h`) asks the machine to load
another program of the host directory in place of this one, and returns only if it could not. Under the shell, the
shell comes back when that program ends, with `CERES_STATUS` set to its exit status and `PWD` still saying where it
was.

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
the tests must print was written for the defaults.

The shell is tested by sessions: each `tests/shell/<session>.type` is typed on it (`ceres run`, with `CERES_PATH=build/shell`,
the host directory a copy of `tests/shell/files` with `tests/shell/*.c` built into its `games/`), and what it printed,
its error stream and its exit status are compared with `<session>.expected`, `.stderr` and `.status`.

A test is `tests/<name>.c`; what it must print is `tests/expected/<name>.expected`, byte for byte. Beside it can be
`.stderr` (its error stream), `.status` (its exit status), `.stdin` (what is typed on its terminal), `.screen` (what
a test that draws on the text plane shows, frame by frame), `.flags` (compiler options -
the library is then compiled with them too), `.run` (more for `ceres run`: `--env`, `--host-dir build/host`,
`-- arguments`) and `.ports` (sticks and cartridges to plug in). A `// USE: irq` line near the top links an
optional module.

An example, `examples/<name>.c`, is always compiled; with `examples/expected/<name>.expected` it is also run and its
output compared. Beside it can be `.flags` (compiler options: `-DDEMO_FRAMES=...` for a demo that plays itself),
`.stdin`, `.status`, `.run` (more for `ceres run`, as `--profile micro`) and `.frames`: the screens the example
presents, recorded with `--frames`, one line each - the first 16 hex digits of the PNG's SHA-256. The retro examples
(`maze.c` on `micro`, `platformer.c` on `retro`) are checked that way. The runners also make every run listed in
`tools/img2tiles.cases` and compare it with the file it names: the tests of `tools/img2tiles.js` and the headers of
the examples' pictures (`examples/art`).

Every run is headless (`ceres run --headless`): what the program writes to its terminal is read back from
`--transcript`, the `.stdin` file is typed with `--type` and the screens come from `--screen-log`. A program never
writes to the host's terminal, so a run that leaves anything on the host's stdout (besides ceresc's `Wrote` lines)
fails.

## Tools

| Script | What it does |
| --- | --- |
| `Makefile`, `CMakeLists.txt`, `CMakePresets.json` | The build: `make help`. |
| `tools/cmake/*.cmake` | The build's steps: header dependencies, merging the declarations, the verify tests. |
| `tools/mklib.ps1` | Builds the library archives per optimization level, in PowerShell. |
| `tools/runtests.js`, `tools/runtests.ps1` | The test runner. |
| `tools/gendocs.js` | Writes `docs/reference` from the headers. |
| `tools/img2tiles.js` | A PNG into what the GPU's level V2 draws: a palette, tiles and a map for a tile layer (`ceres/tiles.h`), or sprite pictures (`ceres/sprite.h`), as C or CASM. `node tools/img2tiles.js --help`. |
| `tools/mkpack.js` | Builds a resource pack (`ceres/pack.h`) from host files, as a cartridge image. |
| `tools/gen_font.js` | Generates the 8x8 font (`src/ceres/font_data.inc`), and the VM's text-window font. |
| `tools/gen_tables.js` | The constant tables the sources include (the sine of `ceres/fixed.h`, the CRC-32 table). |
| `tools/gen_math_tables.js`, `tools/gen_math64_tables.js`, `tools/gen_f64_vectors.js` | Reference values for the `float` and `double` math tests and the binary64 arithmetic vectors. |
| `tools/example.ps1` | Builds one program from `examples/` and runs it. |
