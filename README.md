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

```sh
powershell tools/mklib.ps1                      # build/lib/O0, O1 and O2: libceres.car and libceres.decls.casm
powershell tools/mklib.ps1 -SoftDouble          # the same for -fsoft-double programs, in build/lib/O<n>-sd
powershell tools/install.ps1 -Prefix <dir>      # a sysroot: <dir>/include and <dir>/lib
```

`ceresc` and `ceres` are found next to this checkout (`../../Ceres-C`, `../../CeresASM`) or through the `CERESC`
and `CERES_PATH` environment variables.

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
node tools/runtests.js                  # every test at -O0, -O1 and -O2, each header alone, the examples
node tools/runtests.js --test test_json --levels 2
node tools/runtests.js --update         # write tests/expected/<name>.expected from what the tests print
powershell tools/runtests.ps1           # the same runner in PowerShell (-Test, -Levels, -Update, -GcSections)
```

A test is `tests/<name>.c`; what it must print is `tests/expected/<name>.expected`, byte for byte. Beside it can be
`.stderr` (its error stream), `.status` (its exit status), `.stdin` (what it reads), `.flags` (compiler options -
the library is then compiled with them too), `.run` (more for `ceres run`: `--env`, `--host-dir build/host`,
`-- arguments`) and `.ports` (sticks and cartridges to plug in). A `// USE: irq` line near the top links an
optional module.

## Tools

| Script | What it does |
| --- | --- |
| `tools/mklib.ps1` | Builds the library archives, per optimization level. |
| `tools/install.ps1` | Lays out a sysroot for `ceresc --sysroot`. |
| `tools/runtests.js`, `tools/runtests.ps1` | The test runner. |
| `tools/gendocs.js` | Writes `docs/reference` from the headers. |
| `tools/mkpack.js` | Builds a resource pack (`ceres/pack.h`) from host files, as a cartridge image. |
| `tools/gen_font.js` | Generates the 8x8 font (`src/ceres/font_data.inc`), and the VM's text-window font. |
| `tools/gen_tables.js` | The constant tables the sources include (the sine of `ceres/fixed.h`, the CRC-32 table). |
| `tools/gen_math_tables.js`, `tools/gen_f64_vectors.js` | Reference values for the math and soft-double tests. |
| `tools/example.ps1` | Builds one program from `examples/` and runs it. |
| `tools/consolecheck.ps1` | Checks the text interface against a real Windows console. |
