#!/usr/bin/env node
// The test runner, for any system node runs on (tools/runtests.ps1 is the same for PowerShell).
//
//   node tools/runtests.js                        everything: every test at -O0, -O1 and -O2, the headers, the examples
//   node tools/runtests.js --test test_malloc     one test (or several: --test a,b)
//   node tools/runtests.js --levels 0,2           only those optimization levels (0, 1, 2, 3, s, g)
//   node tools/runtests.js --update               write tests/expected/<name>.expected from what the tests print
//   node tools/runtests.js --headers              only compile each header alone
//   node tools/runtests.js --from-sources         compile the whole library into every test instead of the archive
//   node tools/runtests.js --gc-sections          link every program leaving out the functions nothing reaches
//   node tools/runtests.js --build-library        only (re)build build/lib/O<level>/ for the levels asked for
//   node tools/runtests.js --library <dir>        test against a library built elsewhere - a CMake build directory
//                                                 (<dir>/libceres.car, libceres.decls.casm, obj/) - at every level;
//                                                 {level} in <dir> is the level (--library build/cmake/O{level})
//
// The tools are found next to this checkout (../../Ceres-C, ../../CeresASM) or through CERESC (ceresc) and
// CERES_PATH (ceres: its directory, or the executable). What a test is compared against lives in tests/expected/:
//   <name>.expected  what it prints (compared byte for byte; the driver's "Wrote" lines are left out)
//   <name>.stderr    what it writes to its error stream (else it must write nothing there)
//   <name>.status    the exit status it must end with (0 when there is none)
//   <name>.stdin     what it reads from the terminal
//   <name>.flags     compiler flags for it - and then the library is compiled with them, from its sources
//   <name>.run       more words for `ceres run` (--env, --host-dir build/host, -- arguments)
//   <name>.ports     media to plug in: `--port 0=file` or `--cart 1=file`, one a line
// and a `// USE: irq` line near the top of a test links an optional module (irq, fault, mmu).
"use strict";
const fs = require("fs");
const path = require("path");
const { spawnSync } = require("child_process");

const Root = path.resolve(__dirname, "..");
process.chdir(Root);
const exe = process.platform === "win32" ? ".exe" : "";

// ---- options ----
const args = process.argv.slice(2);
const opt = { tests: [], levels: ["0", "1", "2"], headers: false, update: false, fromSources: false, gc: false, buildOnly: false, timeout: 180, library: null };
const LevelOrder = ["0", "1", "2", "3", "s", "g"];                 // what -O takes
const byLevel = (a, b) => LevelOrder.indexOf(a) - LevelOrder.indexOf(b);
const valueOf = (i) => {
    if (i >= args.length) { console.error(`${args[i - 1]} needs a value`); process.exit(2); }
    return args[i];
};
for (let i = 0; i < args.length; i++) {
    const a = args[i];
    if (a === "--test") opt.tests.push(...valueOf(++i).split(",").filter(Boolean));
    else if (a === "--levels") opt.levels = valueOf(++i).split(/[,; ]+/).filter(Boolean);
    else if (a === "--library") opt.library = path.resolve(valueOf(++i));
    else if (a === "--headers") opt.headers = true;
    else if (a === "--update") opt.update = true;
    else if (a === "--from-sources") opt.fromSources = true;
    else if (a === "--gc-sections") opt.gc = true;
    else if (a === "--build-library") opt.buildOnly = true;
    else if (a === "--timeout") opt.timeout = Number(valueOf(++i));
    else { console.error(`unknown option ${a}`); process.exit(2); }
}
for (const level of opt.levels)
    if (!LevelOrder.includes(level)) { console.error(`--levels: '${level}' is not a level (0, 1, 2, 3, s, g)`); process.exit(2); }

// ---- the tools ----
function onPath(name) {
    for (const dir of (process.env.PATH || "").split(path.delimiter)) {
        const candidate = path.join(dir, name + exe);
        if (dir && fs.existsSync(candidate)) return candidate;
    }
    return null;
}
function findCeresc() {
    if (process.env.CERESC && fs.existsSync(process.env.CERESC)) return path.resolve(process.env.CERESC);
    for (const build of ["gcc", "ninja", "msvc", "clang", ""]) {
        for (const tail of [["bin", "Release"], ["bin"]]) {
            const candidate = path.join(Root, "..", "..", "Ceres-C", "build", build, ...tail, "ceresc" + exe);
            if (fs.existsSync(candidate)) return path.resolve(candidate);
        }
    }
    const found = onPath("ceresc");
    if (found) return found;
    throw new Error("cannot find ceresc - set CERESC or build it next to this checkout");
}
function findCeresDir() {
    for (const where of [process.env.CERES_PATH, process.env.CERES_DIR]) {
        if (where && fs.existsSync(where)) return fs.statSync(where).isDirectory() ? path.resolve(where) : path.dirname(path.resolve(where));
    }
    const sibling = path.join(Root, "..", "..", "CeresASM");
    if (fs.existsSync(path.join(sibling, "ceres" + exe))) return path.resolve(sibling);
    const found = onPath("ceres");
    if (found) return path.dirname(found);
    throw new Error("cannot find ceres - set CERES_PATH or build it next to this checkout");
}
const Ceresc = findCeresc();
const CeresDir = findCeresDir();
const Ceres = path.join(CeresDir, "ceres" + exe);
process.env.CERES_HEADLESS = "1";      // frames of the text framebuffer go to the terminal, where they are compared

// ---- the sources ----
const Optional = {
    irq: ["src/ceres/irq.c"],
    fault: ["src/ceres/fault.c", "asm/optional/fault.casm"],
    mmu: ["src/ceres/mmu_fault.c", "asm/optional/mmu_fault.casm"],
};
const OptionalFiles = Object.values(Optional).flat();
function walk(dir, ext, out = []) {
    for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
        const p = path.posix.join(dir, entry.name);
        if (entry.isDirectory()) walk(p, ext, out);
        else if (p.endsWith(ext)) out.push(p);
    }
    return out;
}
const AllC = walk("src", ".c").sort();
const CoreC = AllC.filter((f) => !OptionalFiles.includes(f));
const Asm = fs.readdirSync("asm").filter((f) => f.endsWith(".casm")).map((f) => "asm/" + f).sort();
const OptionalAsm = OptionalFiles.filter((f) => f.endsWith(".casm"));
fs.mkdirSync("build", { recursive: true });

const libraryDir = (level) => (opt.library ? opt.library.split("{level}").join(level) : `build/lib/O${level}`);
const flatName = (p) => p.replace(/\.[^./\\]+$/, "").replace(/[/\\.]/g, "_");

// ---- running a tool ----
// Its stdout and stderr into files, byte for byte; stdin from a file when there is one. The exit status, or -1 when
// it ran out of time.
//
// ceresc starts the VM as a child of its own, and a VM left running keeps the output pipes open, so a timeout has to
// end both. On Windows that is taskkill /T afterwards. Elsewhere spawnSync would wait on the pipes the orphaned VM
// holds, so the tool runs under coreutils' timeout, which signals its whole process group; without it, only Node's
// own timeout is left.
const posixTimeout = process.platform !== "win32" && spawnSync("timeout", ["--version"]).status === 0;
function run(tool, argv, outFile, errFile, stdinFile) {
    const options = { cwd: Root, timeout: opt.timeout * 1000, maxBuffer: 1 << 30, windowsHide: true };
    if (stdinFile) options.input = fs.readFileSync(stdinFile);
    if (posixTimeout) {
        options.timeout += 10000;                                   // coreutils' goes first
        argv = ["-s", "KILL", String(opt.timeout), tool, ...argv];
        tool = "timeout";
    }
    const r = spawnSync(tool, argv, options);
    fs.writeFileSync(outFile, r.stdout || Buffer.alloc(0));
    fs.writeFileSync(errFile, r.stderr || Buffer.alloc(0));
    if (r.error && r.error.code === "ETIMEDOUT") {
        if (process.platform === "win32" && r.pid) spawnSync("taskkill", ["/T", "/F", "/PID", String(r.pid)]);   // and the VM it started
        return -1;
    }
    if (r.error) throw r.error;
    if (posixTimeout && (r.status === 124 || r.status === 137 || r.signal === "SIGKILL")) return -1;
    return r.status === null ? -1 : r.status;
}
const readText = (p) => (fs.existsSync(p) ? fs.readFileSync(p).toString("latin1") : null);
const programOutput = (raw) => raw.replace(/^(Wrote [^\r\n]*\r?\n)+/, "").replace(/\r\n/g, "\n");
const lf = (s) => s.replace(/\r\n/g, "\n");
const splitWords = (s) => s.trim().split(/\s+/).filter(Boolean);

// ---- the library (what tools/mklib.ps1 does) ----
function newestInputTime() {
    let newest = 0;
    const visit = (p) => {
        const st = fs.statSync(p);
        if (st.isDirectory()) for (const e of fs.readdirSync(p)) visit(path.join(p, e));
        else if (!p.endsWith(".casm") || p.startsWith("asm")) newest = Math.max(newest, st.mtimeMs);
    };
    for (const p of ["src", "asm", "include", "tools/mklib.ps1", "tools/runtests.js"]) visit(p);
    return Math.max(newest, fs.statSync(Ceresc).mtimeMs, fs.statSync(Ceres).mtimeMs);
}
function libraryFresh(level) {
    const archive = `${libraryDir(level)}/libceres.car`;
    if (!fs.existsSync(archive) || !fs.existsSync(`${libraryDir(level)}/libceres.decls.casm`)) return false;
    return fs.statSync(archive).mtimeMs >= newestInputTime();
}
function removeGenerated() {
    for (const f of walk("src", ".casm")) fs.rmSync(f, { force: true });
    for (const f of fs.readdirSync("tests").filter((f) => f.endsWith(".casm"))) fs.rmSync(path.join("tests", f), { force: true });
}
function buildLibrary(level) {
    const dir = libraryDir(level);
    fs.rmSync(dir, { recursive: true, force: true });
    fs.mkdirSync(`${dir}/obj`, { recursive: true });
    console.log(`-O${level} : compiling ${AllC.length} C files`);
    let code = run(Ceresc, [...AllC, "-I", "include", `-O${level}`, "-Werror", "-S", "-o", `${dir}/libceres.cres`], `${dir}/compile.out`, `${dir}/compile.err`);
    if (code !== 0) throw new Error(`ceresc could not compile the library at -O${level}:\n${readText(`${dir}/compile.err`)}`);
    const units = [...AllC.map((f) => f.replace(/\.c$/, ".casm")), ...Asm, ...OptionalAsm];
    console.log(`-O${level} : assembling ${units.length} units`);
    for (const u of units) {
        code = run(Ceres, ["asm", "-c", u, "-o", `${dir}/obj/${flatName(u)}.cobj`], `${dir}/asm.out`, `${dir}/asm.err`);
        if (code !== 0) throw new Error(`ceres asm -c ${u}:\n${readText(`${dir}/asm.err`)}`);
    }
    const objects = fs.readdirSync(`${dir}/obj`).filter((f) => f.endsWith(".cobj")).sort().map((f) => `${dir}/obj/${f}`);
    code = run(Ceres, ["ar", `${dir}/libceres.car`, ...objects], `${dir}/ar.out`, `${dir}/ar.err`);
    if (code !== 0) throw new Error(`ceres ar:\n${readText(`${dir}/ar.err`)}`);
    console.log(`${dir}/libceres.car: ${objects.length} objects, ${fs.statSync(`${dir}/libceres.car`).size} bytes`);
    removeGenerated();
}
function ensureLibrary(levels) {
    for (const level of levels) if (!libraryFresh(level)) buildLibrary(level);
}
function libraryArgs(level, modules) {
    const dir = libraryDir(level);
    const objects = [];
    for (const m of modules) {
        if (!Optional[m]) throw new Error(`unknown module '${m}'`);
        for (const f of Optional[m]) objects.push(`${dir}/obj/${flatName(f)}.cobj`);
    }
    return [...objects, `${dir}/libceres.car`, "--decls", `${dir}/libceres.decls.casm`];
}
function modulesOf(file) {
    const use = [];
    for (const line of fs.readFileSync(file, "latin1").split(/\r?\n/).slice(0, 6)) {
        const m = line.match(/^\s*\/\/\s*USE:\s*(.+)$/);
        if (m) use.push(...splitWords(m[1]));
    }
    return use;
}

// ---- reporting ----
const color = (code, s) => (process.stdout.isTTY ? `\x1b[${code}m${s}\x1b[0m` : s);
const red = (s) => color(31, s), green = (s) => color(32, s), yellow = (s) => color(33, s), cyan = (s) => color(36, s);
function showBytes(s) {
    return [...s].map((ch) => {
        const n = ch.charCodeAt(0);
        if (n === 10) return "\\n";
        if (n === 0) return "\\0";
        if (n < 32 || n > 126) return "\\x" + n.toString(16).toUpperCase().padStart(2, "0");
        return ch;
    }).join("");
}
function showDifference(expected, actual) {
    let at = 0;
    while (at < expected.length && at < actual.length && expected[at] === actual[at]) at++;
    const from = Math.max(0, at - 30);
    console.log(yellow(`      first difference at byte ${at} (expected ${expected.length} bytes, got ${actual.length})`));
    console.log(yellow(`      expected: ${showBytes(expected.slice(from, from + 80))}`));
    console.log(yellow(`      actual:   ${showBytes(actual.slice(from, from + 80))}`));
}

const failures = [];
let passed = 0;

function testHeaders() {
    console.log(cyan("headers: each one compiled alone"));
    fs.mkdirSync("build/each", { recursive: true });
    const headers = walk("include", ".h").map((h) => h.slice("include/".length)).sort();
    let bad = 0;
    for (const rel of headers) {
        const flat = rel.replace(/[/.]/g, "_");
        const src = `build/each/${flat}.c`;
        fs.writeFileSync(src, `#include "${rel}"\nint main(void) { return 0; }\n`);
        const code = run(Ceresc, [src, "-I", "include", "-Werror", "-S", "-o", `build/each/${flat}.casm`], `build/each/${flat}.out`, `build/each/${flat}.err`);
        if (code !== 0) {
            bad++;
            failures.push(`header ${rel}`);
            console.log(red(`  FAIL  ${rel}`));
            for (const l of (readText(`build/each/${flat}.err`) || "").split(/\r?\n/).slice(0, 4)) console.log(yellow(`      ${l}`));
        }
    }
    if (bad === 0) {
        console.log(green(`  ok    ${headers.length} headers`));
        passed++;
    }
}

function testExamples() {
    console.log(cyan("examples: build, and run those with an expected output"));
    fs.mkdirSync("build/examples", { recursive: true });
    const examples = fs.readdirSync("examples").filter((f) => f.endsWith(".c")).map((f) => f.slice(0, -2)).sort();
    let bad = 0;
    for (const name of examples) {
        const use = modulesOf(`examples/${name}.c`);
        const extra = use.flatMap((u) => { if (!Optional[u]) throw new Error(`examples/${name}.c asks for an unknown module ${u}`); return Optional[u]; });
        const flagsFile = `examples/expected/${name}.flags`;
        const flags = fs.existsSync(flagsFile) ? splitWords(readText(flagsFile)) : [];
        const expectedPath = `examples/expected/${name}.expected`;
        const stdin = fs.existsSync(`examples/expected/${name}.stdin`) ? `examples/expected/${name}.stdin` : null;
        const statusFile = `examples/expected/${name}.status`;
        const wantStatus = fs.existsSync(statusFile) ? Number(readText(statusFile).trim()) : 0;
        const sources = [...CoreC, ...extra, ...Asm, `examples/${name}.c`];
        let code;
        if (fs.existsSync(expectedPath)) {
            const body = opt.fromSources ? [...sources, ...flags] : [`examples/${name}.c`, ...libraryArgs(2, use), ...flags];
            code = run(Ceresc, [...body, "-I", "include", "-O2", "-Werror", "-o", `build/examples/${name}.cres`, ...(opt.gc ? ["--gc-sections"] : []),
                "--run", "--clean", "--ceres-path", CeresDir], `build/examples/${name}.out`, `build/examples/${name}.err`, stdin);
        } else {
            code = run(Ceresc, [...sources, "-I", "include", "-O2", "-Werror", "-S", "-o", `build/examples/${name}.casm`], `build/examples/${name}.out`, `build/examples/${name}.err`);
        }
        if (code !== wantStatus) {
            bad++;
            failures.push(`example ${name} (build or run, exit ${code})`);
            console.log(red(`  FAIL  ${name}  does not build or run`));
            continue;
        }
        if (!fs.existsSync(expectedPath)) continue;
        const actual = programOutput(readText(`build/examples/${name}.out`) || "");
        if (opt.update) {
            fs.writeFileSync(expectedPath, Buffer.from(actual, "latin1"));
            console.log(yellow(`  wrote ${expectedPath} (${actual.length} bytes)`));
        } else if (lf(readText(expectedPath)) !== actual) {
            bad++;
            failures.push(`example ${name} (output)`);
            console.log(red(`  FAIL  ${name}  output differs from ${expectedPath}`));
            showDifference(lf(readText(expectedPath)), actual);
        }
    }
    if (bad === 0) {
        console.log(green(`  ok    ${examples.length} examples`));
        passed++;
    }
}

function recreateHostDirectory() {
    fs.rmSync("build/host", { recursive: true, force: true });
    fs.mkdirSync("build/host", { recursive: true });
    if (fs.existsSync("tests/data/host")) fs.cpSync("tests/data/host", "build/host", { recursive: true });
}

function testOne(name) {
    const src = `tests/${name}.c`;
    const use = modulesOf(src);
    const extra = use.flatMap((u) => { if (!Optional[u]) throw new Error(`${src} asks for an unknown module '${u}'`); return Optional[u]; });
    const sources = [...CoreC, ...extra, ...Asm, src];
    const flagsFile = `tests/expected/${name}.flags`;
    const testFlags = fs.existsSync(flagsFile) ? splitWords(readText(flagsFile)) : [];
    const expectedPath = `tests/expected/${name}.expected`;
    const fromSource = opt.fromSources || testFlags.length > 0;        // a library option means a library built with it
    let reference = null;
    let referenceLevel = null;                          // the first level that ran: what the others must print
    for (const level of opt.levels) {
        const label = `${name.padEnd(22)} -O${level}`;
        const out = `build/${name}.O${level}.out`;
        const err = `build/${name}.O${level}.err`;
        const body = fromSource ? [...sources, ...testFlags] : [src, ...libraryArgs(level, use)];
        const cmd = [...body, "-I", "include", `-O${level}`, "-Werror", "-o", `build/${name}.O${level}.cres`, ...(opt.gc ? ["--gc-sections"] : []),
            "--run", "--clean", "--ceres-path", CeresDir];
        const portsFile = `tests/expected/${name}.ports`;
        if (fs.existsSync(portsFile)) {
            fs.mkdirSync("build/ports", { recursive: true });
            for (const f of fs.readdirSync("build/ports")) fs.rmSync(path.join("build/ports", f), { recursive: true, force: true });
            for (const spec of readText(portsFile).split(/\r?\n/).filter((l) => l.trim())) {
                const pair = spec.trim().match(/^(\S+)\s+(.+)$/);        // the flag, then the rest as its value
                if (!pair) throw new Error(`${portsFile}: expected "<flag> <value>", got "${spec.trim()}"`);
                cmd.push("--run-arg", pair[1], "--run-arg", pair[2]);
            }
        }
        const runFile = `tests/expected/${name}.run`;
        if (fs.existsSync(runFile)) {
            const text = readText(runFile);
            for (const word of splitWords(text)) cmd.push("--run-arg", word);
            if (/--host-dir\s+build\/host(\s|$)/.test(text)) recreateHostDirectory();
        }
        const stdin = fs.existsSync(`tests/expected/${name}.stdin`) ? `tests/expected/${name}.stdin` : null;
        const code = run(Ceresc, cmd, out, err, stdin);
        const errText = readText(err) || "";
        const statusFile = `tests/expected/${name}.status`;
        const wantStatus = fs.existsSync(statusFile) ? Number(readText(statusFile).trim()) : 0;
        if (code !== wantStatus) {
            failures.push(`${name} -O${level} (exit ${code})`);
            console.log(red(`  FAIL  ${label}  the build or the run failed (exit ${code})`));
            for (const l of errText.split(/\r?\n/).filter((l) => l && !l.startsWith("Wrote ")).slice(0, 6)) console.log(yellow(`      ${l}`));
            continue;
        }
        const actual = programOutput(readText(out) || "");
        if (opt.update && level === opt.levels[0]) {
            fs.writeFileSync(expectedPath, Buffer.from(actual, "latin1"));
            console.log(yellow(`  wrote ${expectedPath} (${actual.length} bytes)`));
        }
        if (reference === null) { reference = actual; referenceLevel = level; }
        else if (actual !== reference) {
            failures.push(`${name} -O${level} (differs from -O${referenceLevel})`);
            console.log(red(`  FAIL  ${label}  prints something different from -O${referenceLevel}`));
            showDifference(reference, actual);
            continue;
        }
        const errActual = programOutput(errText).split(/(?<=\n)/).filter((l) => !/^(Wrote |  warning \[)/.test(l)).join("");
        const errPath = `tests/expected/${name}.stderr`;
        let errExpected = fs.existsSync(errPath) ? lf(readText(errPath)) : "";
        if (opt.update && level === opt.levels[0]) {
            if (errActual === "") fs.rmSync(errPath, { force: true });
            else fs.writeFileSync(errPath, Buffer.from(errActual, "latin1"));
            errExpected = errActual;
        }
        if (errExpected !== errActual) {
            failures.push(`${name} -O${level} (stderr)`);
            console.log(red(`  FAIL  ${label}  its error stream differs from ${errPath}`));
            showDifference(errExpected, errActual);
            continue;
        }
        const expected = readText(expectedPath);
        if (expected === null) {
            failures.push(`${name} (no ${expectedPath})`);
            console.log(red(`  FAIL  ${label}  there is no ${expectedPath} (run with --update, then review it)`));
        } else if (lf(expected) !== actual) {
            failures.push(`${name} -O${level} (output)`);
            console.log(red(`  FAIL  ${label}  output differs from the expected file`));
            showDifference(lf(expected), actual);
        } else {
            passed++;
            console.log(green(`  ok    ${label}`));
        }
    }
}

// ---- main ----
console.log(`ceresc  ${Ceresc}`);
console.log(`ceres   ${CeresDir}`);
if (opt.library)
    for (const level of [...new Set([...opt.levels, "2"])])      // the examples are built at -O2
        if (!fs.existsSync(path.join(libraryDir(level), "libceres.car")))
            throw new Error(`--library: there is no libceres.car in ${libraryDir(level)}`);
if (opt.buildOnly) {
    for (const level of opt.levels) buildLibrary(level);
    process.exit(0);
}
if (opt.headers) {
    testHeaders();
    process.exit(failures.length ? 1 : 0);
}
let tests = fs.readdirSync("tests").filter((f) => f.endsWith(".c")).map((f) => f.slice(0, -2)).sort();
if (opt.tests.length) tests = tests.filter((t) => opt.tests.includes(t));
if (!tests.length) throw new Error("no tests match");
fs.mkdirSync("tests/expected", { recursive: true });
if (!opt.fromSources && !opt.library) ensureLibrary([...new Set([...opt.levels, "2"])].sort(byLevel));
for (const name of tests) testOne(name);
testHeaders();
testExamples();
console.log("");
if (!failures.length) {
    console.log(green(`all tests passed (${passed} checks: ${tests.length} tests x ${opt.levels.length} levels, plus the headers)`));
    process.exit(0);
}
console.log(red(`${failures.length} failure(s):`));
for (const f of failures) console.log(red(`  - ${f}`));
process.exit(1);
