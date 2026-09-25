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
  with `mklib -SoftDouble` prints and reads it whole. The math functions are float either way.
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
