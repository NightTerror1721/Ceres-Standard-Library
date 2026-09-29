<#
.SYNOPSIS
    Builds and runs the standard library's tests, and compares what they print.

.DESCRIPTION
    A test's verdict is its TEXT, and its exit status: `ceres run` exits with what main returned, which
    must be 0 unless tests/expected/<name>.status says otherwise (a number on one line). For every
    tests/<name>.c this compiles the test at -O0, -O1 and -O2 and links it against the library built at the
    same level (tools/mklib.ps1 builds build/lib/O<level>/libceres.car when it is missing or older than the
    sources, and ceresc takes the archive with --decls), runs it, and requires the output to equal
    tests/expected/<name>.expected BYTE
    FOR BYTE (line endings aside: the Windows host turns "\n" into "\r\n"). A stray NUL - the
    signature of a word store into a byte register - therefore fails a test. All three levels must
    print the same thing: that is how an optimizer bug becomes a failing build instead of a surprise.

    Optional modules (irq, fault) are not linked unless a test asks for them with a `// USE: irq` line
    in its first lines. They bind interrupt vectors, and the linker allows one binding per number for the
    whole program, so a program that binds its own must not carry them.

    A test with a tests/expected/<name>.ports file has media plugged into the machine's peripheral ports: one
    `--port 0=build/ports/stick.img` or `--cart 1=tests/data/game.cart` per line (a stick's file is created when
    it is not there, and is new for every level).

    A test with a tests/expected/<name>.run file passes what it holds to `ceres run`: `--env NAME=value`, and
    `-- a b` for main's arguments. Words are split at white space, so no value can contain any. With `--host-dir build/host` the directory is made afresh for every level,
    with a copy of tests/data/host in it.

    A program runs without a window, as fast as the host goes (`ceres run --headless --speed max --gpu software`),
    and never writes to the host's stdout (CeresASM plan/v2 F5.7): what it wrote to its terminal comes back from
    --transcript, where the error stream's bytes are between ESC[E and ESC[e. tests/expected/<name>.stdin is typed
    on its terminal (--type), through the line discipline, as the program reads.

    What a test writes to its error stream (stderr, perror, assert, abort), then what `ceres` itself says, is
    compared with tests/expected/<name>.stderr, and must be empty when there is none.

    A test that includes ceres/text.h or ceres/tui.h also has its screens compared: the text plane at every
    Present and at the end (--screen-log), with tests/expected/<name>.screen.

    The shell (shell/shell.c) is built as `make` builds it, into build/shell/shell/shell.cres, and each
    tests/shell/<session>.type is typed on it - `ceres run` with no program and CERES_PATH=build/shell, the host
    directory a copy of tests/shell/files with tests/shell/*.c built into it - and what it printed compared with
    <session>.expected, what went to its error stream with <session>.stderr (else nothing) and its exit status with
    <session>.status.

    An example (examples/<name>.c) with examples/expected/<name>.expected is run and its output compared the same
    way; examples/expected/<name>.flags holds compiler flags for it (-DDEMO_FRAMES=...), <name>.run more words for
    `ceres run` (--profile micro), and <name>.frames the hashes of the screens it presents: it runs with `--frames`,
    and the first 16 hex digits of each PNG's SHA-256, in order, must be the lines of that file.

    tools/img2tiles.cases lists runs of tools/img2tiles.js and the file each must give, byte for byte: the tool's
    own tests (tests/img2tiles) and the headers of the examples' pictures (examples/art).

    A test with a tests/expected/<name>.flags file sets a compile-time option of the LIBRARY (-DCERES_...), so
    the library is compiled again with it, together with the test, as before. -FromSources does that for every
    test: the slow path, and the one that proves the archive changes nothing. A tests/expected/<name>.cflags file
    holds flags for the test program alone (-fshort-double), which is then always linked against the archive.

    The tools are found the way ceresc finds ceres: the CERES and CERESC environment variables name them (the
    executable, or the directory that holds it), else the directory CERES_PATH names (where Ceres is installed),
    else PATH. A variable that is set decides.

.EXAMPLE
    tools\runtests.ps1                      # everything
    tools\runtests.ps1 -Test test_malloc    # one test
    tools\runtests.ps1 -Headers             # only "each header compiles on its own"
    tools\runtests.ps1 -Update              # write tests/expected from the -O0 output (review it!)
    tools\runtests.ps1 -FromSources         # compile the whole library into every test, as it once was
    tools\runtests.ps1 -GcSections          # link every program leaving out the functions nothing reaches
#>
[CmdletBinding()]
param(
    [string[]]$Test = @(),
    [string]$Levels = "0,1,2",          # optimization levels, e.g. -Levels 0,2
    [switch]$Headers,
    [switch]$Update,
    [switch]$FromSources,
    [switch]$GcSections,                # link every program with --gc-sections (the functions nothing reaches left out)
    [int]$TimeoutSeconds = 180
)

# `powershell -File x.ps1 -Test a,b` delivers "a,b" as ONE string: split it here.
$Test = @($Test | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$LevelList = @(($Levels -join ',') -split '[,; ]+' | Where-Object { $_ } | ForEach-Object { [int]$_ })

$LinkFlags = if ($GcSections) { "--gc-sections" } else { "" }
. "$PSScriptRoot/common.ps1"    # $Root, $Ceresc, $CeresDir, the source lists, Invoke-Tool, Same, ...

# No window, whatever ceresc starts.
$env:CERES_HEADLESS = '1'

# ---- a program's run (CeresASM plan/v2 F5.7) ------------------------------------------------------

# The words for `ceres run`, each after a --run-arg: no window, flat out, the transcript and, when asked for, the
# screen log and what is typed.
function Get-RunArgs([string]$transcript, [string]$screenLog, [string]$typed) {
    $words = @('--headless', '--speed', 'max', '--gpu', 'software', '--transcript', $transcript)
    if ($screenLog) { $words += @('--screen-log', $screenLog) }
    if ($typed) { $words += @('--type', $typed) }
    return (($words | ForEach-Object { "--run-arg $_" }) -join ' ')
}

# The transcript's two streams: the error stream's bytes are between ESC [ E and ESC [ e.
function Split-Transcript([string]$raw) {
    $out = New-Object System.Text.StringBuilder
    $err = New-Object System.Text.StringBuilder
    $inError = $false
    for ($i = 0; $i -lt $raw.Length; $i++) {
        if ($raw[$i] -eq [char]27 -and $i + 2 -lt $raw.Length -and $raw[$i + 1] -eq '[' -and ($raw[$i + 2] -ceq 'E' -or $raw[$i + 2] -ceq 'e')) {
            $inError = $raw[$i + 2] -ceq 'E'
            $i += 2
        }
        elseif ($inError) { [void]$err.Append($raw[$i]) }
        else { [void]$out.Append($raw[$i]) }
    }
    return @{ Out = $out.ToString(); Err = $err.ToString() }
}

# Whether a program draws on the text plane, so its screens are compared.
# What the run wrote to the host's stdout, without ceresc's "Wrote" lines: nothing, ever - a program's output goes to
# its terminal, never to the host's (CeresASM plan/v2 SPEC 1.4, F5.12).
function Get-HostOutput([string]$outFile) {
    $text = Read-Text $outFile
    if ($null -eq $text) { return '' }
    return (((Get-ProgramOutput $text) -split "(?<=`n)" | Where-Object { $_ -notmatch '^Wrote ' }) -join '')
}

function Test-UsesTextPlane([string]$file) {
    return [bool](Select-String -Path $file -Pattern '#include\s+"ceres/(text|tui)\.h"' -Quiet)
}

# ---- reporting differences ----------------------------------------------------------------------

function Show-Bytes([string]$s) {
    $sb = New-Object System.Text.StringBuilder
    foreach ($ch in $s.ToCharArray()) {
        $n = [int]$ch
        if ($n -eq 10) { [void]$sb.Append('\n') }
        elseif ($n -eq 0) { [void]$sb.Append('\0') }
        elseif ($n -lt 32 -or $n -gt 126) { [void]$sb.Append(('\x{0:X2}' -f $n)) }
        else { [void]$sb.Append($ch) }
    }
    return $sb.ToString()
}

function Show-Difference([string]$expected, [string]$actual) {
    $n = [Math]::Min($expected.Length, $actual.Length)
    $at = 0
    while ($at -lt $n -and $expected[$at] -ceq $actual[$at]) { $at++ }
    $from = [Math]::Max(0, $at - 30)
    $e = $expected.Substring($from, [Math]::Min(80, $expected.Length - $from))
    $a = $actual.Substring($from, [Math]::Min(80, $actual.Length - $from))
    Write-Host "      first difference at byte $at (expected $($expected.Length) bytes, got $($actual.Length))" -ForegroundColor DarkYellow
    Write-Host "      expected: $(Show-Bytes $e)" -ForegroundColor DarkYellow
    Write-Host "      actual:   $(Show-Bytes $a)" -ForegroundColor DarkYellow
}

$failures = New-Object System.Collections.ArrayList
$passed = 0

# ---- each header on its own ---------------------------------------------------------------------

function Test-EachHeader {
    Write-Host "headers: each one compiled alone" -ForegroundColor Cyan
    New-Item -ItemType Directory -Force build/each | Out-Null
    $count = 0
    foreach ($h in (Get-ChildItem include -Recurse -Filter *.h | Sort-Object FullName)) {
        $rel = $h.FullName.Substring((Resolve-Path include).Path.Length + 1).Replace('\', '/')
        $flat = $rel -replace '[/.]', '_'
        $src = "build/each/$flat.c"
        [System.IO.File]::WriteAllText("$Root\$src", "#include `"$rel`"`nint main(void) { return 0; }`n")
        $err = "build/each/$flat.err"
        $code = Invoke-Tool $Ceresc "$src -I include -Werror -S -o build/each/$flat.casm" "build/each/$flat.out" $err
        $count++
        if ($code -ne 0) {
            [void]$failures.Add("header $rel")
            Write-Host "  FAIL  $rel" -ForegroundColor Red
            Get-Content $err | Select-Object -First 4 | ForEach-Object { Write-Host "      $_" -ForegroundColor DarkYellow }
        }
    }
    if (($failures | Where-Object { $_ -like 'header *' }).Count -eq 0) {
        Write-Host "  ok    $count headers" -ForegroundColor Green
        $script:passed++
    }
}

if ($Headers) {
    Test-EachHeader
    if ($failures.Count -gt 0) { exit 1 } else { exit 0 }
}

# ---- the examples -------------------------------------------------------------------------------
# Every examples/*.c must build with the whole library at -O2 -Werror. One that has an
# examples/expected/<name>.expected is also run, with examples/expected/<name>.stdin (if there is one) as
# its input, and its output compared byte for byte.

function Test-Examples {
    Write-Host "examples: build, and run those with an expected output" -ForegroundColor Cyan
    New-Item -ItemType Directory -Force build/examples | Out-Null
    $count = 0
    $bad = 0
    foreach ($ex in (Get-ChildItem examples -Filter *.c | Sort-Object Name)) {
        $name = $ex.BaseName
        $count++
        # optional modules, as for the tests: a `// USE: irq` line in the first lines of the example
        $use = @()
        foreach ($line in (Get-Content "examples/$name.c" -TotalCount 6)) {
            if ($line -match '^\s*//\s*USE:\s*(.+)$') { $use += ($Matches[1].Trim() -split '\s+') }
        }
        $extra = @()
        foreach ($u in $use) {
            if (-not $Optional.ContainsKey($u)) { throw "examples/$name.c asks for an unknown module $u" }
            $extra += $Optional[$u]
        }
        # extra compiler flags for the run that is compared (examples/expected/<name>.flags), e.g. a demo
        # build that plays itself for a fixed number of frames
        $flagsFile = "examples/expected/$name.flags"
        $flags = if (Test-Path $flagsFile) { (Get-Content $flagsFile -Raw).Trim() } else { '' }
        $sources = ($CoreC + $extra + $Asm + "examples/$name.c") -join ' '
        $expectedPath = "examples/expected/$name.expected"
        $stdin = "examples/expected/$name.stdin"
        if (-not (Test-Path $stdin)) { $stdin = '' }
        $transcript = "build/examples/$name.transcript"
        Remove-Item $transcript -Force -ErrorAction SilentlyContinue
        $statusFile = "examples/expected/$name.status"
        $wantStatus = if (Test-Path $statusFile) { [int]((Read-Text $statusFile).Trim()) } else { 0 }
        $framesFile = "examples/expected/$name.frames"
        $framesDir = "build/examples/$name.frames"
        if (Test-Path $expectedPath) {
            # ceresc builds, links and runs in one go; the .stdin file is typed on the program's terminal
            $body = if ($FromSources) { "$sources $flags" } else { "examples/$name.c $(Get-LibraryArgs 2 $use) $flags" }   # a define only the example reads goes with either
            $cmd = "$body -I include -O2 -Werror -o build/examples/$name.cres $LinkFlags --run --clean --ceres-path `"$CeresDir`" $(Get-RunArgs $transcript '' $stdin)"
            $runFile = "examples/expected/$name.run"
            if (Test-Path $runFile) {
                foreach ($word in ((Get-Content $runFile) -join ' ').Trim() -split '\s+') {
                    if ($word) { $cmd += " --run-arg $word" }
                }
            }
            if (Test-Path $framesFile) {
                Remove-Item $framesDir -Recurse -Force -ErrorAction SilentlyContinue
                $cmd += " --run-arg --frames --run-arg $framesDir"
            }
            $code = Invoke-Tool $Ceresc $cmd "build/examples/$name.out" "build/examples/$name.err"
        } else {
            $cmd = "$sources -I include -O2 -Werror -S -o build/examples/$name.casm"   # only prove it compiles
            $code = Invoke-Tool $Ceresc $cmd "build/examples/$name.out" "build/examples/$name.err"
        }
        if ($code -ne $wantStatus) {
            $bad++
            [void]$failures.Add("example $name (build or run, exit $code)")
            Write-Host "  FAIL  $name  does not build or run" -ForegroundColor Red
            (Read-Text "build/examples/$name.err") -split "`r?`n" | Where-Object { $_ -and $_ -notmatch '^Wrote ' } | Select-Object -First 6 | ForEach-Object { Write-Host "      $_" -ForegroundColor DarkYellow }
            continue
        }
        if (-not (Test-Path $expectedPath)) { continue }
        $leaked = Get-HostOutput "build/examples/$name.out"
        if (-not (Same $leaked '')) {
            $bad++
            [void]$failures.Add("example $name (host stdout)")
            Write-Host "  FAIL  $name  wrote to the host's stdout" -ForegroundColor Red
            Show-Difference '' $leaked
            continue
        }
        $raw = Read-Text $transcript
        $actual = (Split-Transcript $(if ($null -eq $raw) { '' } else { $raw })).Out
        if ($Update) {
            [System.IO.File]::WriteAllBytes("$Root\$expectedPath", $Latin1.GetBytes($actual))
            Write-Host "  wrote $expectedPath ($($actual.Length) bytes)" -ForegroundColor Yellow
        }
        elseif (-not (Same $actual ((Read-Text $expectedPath) -replace "`r`n", "`n"))) {
            $bad++
            [void]$failures.Add("example $name (output)")
            Write-Host "  FAIL  $name  output differs from $expectedPath" -ForegroundColor Red
            Show-Difference ((Read-Text $expectedPath) -replace "`r`n", "`n") $actual
            continue
        }
        if (Test-Path $framesFile) {
            $hashes = @(Get-ChildItem $framesDir -Filter *.png -ErrorAction SilentlyContinue | Sort-Object Name |
                ForEach-Object { (Get-FileHash $_.FullName -Algorithm SHA256).Hash.Substring(0, 16).ToLowerInvariant() })
            $got = ($hashes | ForEach-Object { "$_`n" }) -join ''
            if ($Update) {
                [System.IO.File]::WriteAllText("$Root\$framesFile", $got)
                Write-Host "  wrote $framesFile ($($hashes.Count) screens)" -ForegroundColor Yellow
            }
            elseif (-not (Same $got ((Read-Text $framesFile) -replace "`r`n", "`n"))) {
                $bad++
                [void]$failures.Add("example $name (screens)")
                Write-Host "  FAIL  $name  its screens differ from $framesFile (the PNGs are in $framesDir)" -ForegroundColor Red
                Show-Difference ((Read-Text $framesFile) -replace "`r`n", "`n") $got
            }
        }
    }
    if ($bad -eq 0) {
        Write-Host "  ok    $count examples" -ForegroundColor Green
        $script:passed++
    }
}

# ---- img2tiles ----------------------------------------------------------------------------------
# Each line of tools/img2tiles.cases: the file a run of tools/img2tiles.js must give, then its arguments.

function Test-Img2tiles {
    Write-Host "img2tiles: the runs of tools/img2tiles.cases" -ForegroundColor Cyan
    New-Item -ItemType Directory -Force build/img2tiles | Out-Null
    $count = 0
    $bad = 0
    foreach ($line in (Get-Content tools/img2tiles.cases)) {
        if (-not $line.Trim() -or $line.Trim().StartsWith('#')) { continue }
        $words = @($line.Trim() -split '\s+')
        $expected = $words[0]
        $made = "build/img2tiles/$(Split-Path $expected -Leaf)"
        $count++
        & node tools/img2tiles.js @($words[1..($words.Count - 1)]) -o $made 2> build/img2tiles/last.err
        if ($LASTEXITCODE -ne 0) {
            $bad++
            [void]$failures.Add("img2tiles $expected (exit $LASTEXITCODE)")
            Write-Host "  FAIL  $expected  the run failed" -ForegroundColor Red
            Get-Content build/img2tiles/last.err | Select-Object -First 4 | ForEach-Object { Write-Host "      $_" -ForegroundColor DarkYellow }
            continue
        }
        $got = (Read-Text $made) -replace "`r`n", "`n"
        if ($Update) {
            Copy-Item $made $expected -Force
            Write-Host "  wrote $expected" -ForegroundColor Yellow
        }
        elseif (-not (Test-Path $expected) -or -not (Same $got ((Read-Text $expected) -replace "`r`n", "`n"))) {
            $bad++
            [void]$failures.Add("img2tiles $expected (output)")
            Write-Host "  FAIL  $expected  differs from what tools/img2tiles.js gives now ($made)" -ForegroundColor Red
        }
    }
    if ($bad -eq 0) {
        Write-Host "  ok    $count runs" -ForegroundColor Green
        $script:passed++
    }
}

# ---- the shell ----------------------------------------------------------------------------------
# The shell as `make` builds it, and the sessions of tests/shell typed on it (see the description above).

function Test-Shell {
    Write-Host "shell: build it, and type each session of tests/shell on it" -ForegroundColor Cyan
    $dir = 'build/shell'
    $hostDir = "$dir/host"
    if (Test-Path $dir) { Remove-Item $dir -Recurse -Force }
    New-Item -ItemType Directory -Force "$dir/shell", "$hostDir/games" | Out-Null
    Copy-Item tests/shell/files/* $hostDir -Recurse -Force
    $problem = Build-Program 'shell/shell.c' "$dir/shell/shell.cres" "$dir/shell-program" 2 $LinkFlags
    foreach ($program in (Get-ChildItem tests/shell -Filter *.c | Sort-Object Name)) {
        if ($problem) { break }
        $problem = Build-Program "tests/shell/$($program.Name)" "$hostDir/games/$($program.BaseName).cres" "$dir/$($program.BaseName)" 2 $LinkFlags
    }
    if ($problem) {
        [void]$failures.Add("shell (build)")
        Write-Host "  FAIL  the shell does not build: $problem" -ForegroundColor Red
        return
    }
    $sessions = @(Get-ChildItem tests/shell -Filter *.type | Sort-Object Name)
    $bad = 0
    # ceres finds the shell in <CERES_PATH>/shell/shell.cres: here, the one just built. The tools were found already.
    $savedCeresPath = $env:CERES_PATH
    $env:CERES_PATH = Join-Path $Root $dir
    try {
    foreach ($session in $sessions) {
        $name = $session.BaseName
        $base = "tests/shell/$name"
        $transcript = "$dir/$name.transcript"
        $cmd = "run --host-dir $hostDir --headless --speed max --gpu software --rtc 2026-09-28T12:00:00 --type $base.type --transcript $transcript"
        $code = Invoke-Tool $Ceres $cmd "$dir/$name.out" "$dir/$name.err"
        $wantStatus = if (Test-Path "$base.status") { [int]((Read-Text "$base.status").Trim()) } else { 0 }
        $raw = Read-Text $transcript
        $streams = Split-Transcript $(if ($null -eq $raw) { '' } else { $raw })
        $errText = Read-Text "$dir/$name.err"
        if ($null -eq $errText) { $errText = '' }
        $errors = $streams.Err + (((Get-ProgramOutput $errText) -split "(?<=`n)" | Where-Object { $_ -notmatch '^(Wrote |  warning \[)' }) -join '')
        if ($Update) {
            [System.IO.File]::WriteAllBytes("$Root\$base.expected", $Latin1.GetBytes($streams.Out))
            if ($errors -ne '') { [System.IO.File]::WriteAllBytes("$Root\$base.stderr", $Latin1.GetBytes($errors)) }
            Write-Host "  wrote $base.expected ($($streams.Out.Length) bytes)" -ForegroundColor Yellow
            continue
        }
        $expected = Read-Text "$base.expected"
        $expected = if ($null -eq $expected) { '' } else { $expected -replace "`r`n", "`n" }
        $expectedErrors = Read-Text "$base.stderr"
        $expectedErrors = if ($null -eq $expectedErrors) { '' } else { $expectedErrors -replace "`r`n", "`n" }
        $problems = @()
        if ($code -ne $wantStatus) { $problems += "exit $code, not $wantStatus" }
        if (-not (Same $streams.Out $expected)) { $problems += 'output' }
        if (-not (Same $errors $expectedErrors)) { $problems += 'error stream' }
        if (-not (Same (Get-HostOutput "$dir/$name.out") '')) { $problems += 'host stdout' }
        if ($problems.Count -gt 0) {
            $bad++
            [void]$failures.Add("shell $name ($($problems -join ', '))")
            Write-Host "  FAIL  shell $($name): $($problems -join ', ')" -ForegroundColor Red
            if ($problems -contains 'output') { Show-Difference $expected $streams.Out }
            if ($problems -contains 'error stream') { Show-Difference $expectedErrors $errors }
        }
    }
    } finally {
        $env:CERES_PATH = $savedCeresPath
    }
    if ($bad -eq 0) {
        Write-Host "  ok    the shell, $($sessions.Count) sessions" -ForegroundColor Green
        $script:passed++
    }
}

# ---- the tests ----------------------------------------------------------------------------------

$tests = @(Get-ChildItem tests -Filter *.c | Sort-Object Name | ForEach-Object { $_.BaseName })
if ($Test.Count -gt 0) { $tests = @($tests | Where-Object { $Test -contains $_ }) }
if ($tests.Count -eq 0) { throw "no tests match" }
New-Item -ItemType Directory -Force tests/expected | Out-Null

Write-Host "ceresc  $Ceresc" -ForegroundColor DarkGray
Write-Host "ceres   $CeresDir" -ForegroundColor DarkGray

# tests/expected/<name>.cflags: flags for compiling the test program only.
function Get-ProgramFlags([string]$name) {
    $file = "tests/expected/$name.cflags"
    if (Test-Path $file) { return (Get-Content $file -Raw).Trim() }
    return ''
}

# The archive, unless every test compiles the library from its sources - a test with a .cflags file never does.
$archivesBuilt = -not $FromSources -or @($tests | Where-Object { (Get-ProgramFlags $_) -ne '' }).Count -gt 0
if ($archivesBuilt) {
    Ensure-Library @($LevelList + 2 | Sort-Object -Unique)   # -O2 also serves the examples and the shell
}

foreach ($name in $tests) {
    $src = "tests/$name.c"
    $use = @()
    foreach ($line in (Get-Content $src -TotalCount 6)) {
        if ($line -match '^\s*//\s*USE:\s*(.+)$') { $use += ($Matches[1].Trim() -split '\s+') }
    }
    $extra = @()
    foreach ($u in $use) {
        if (-not $Optional.ContainsKey($u)) { throw "$src asks for an unknown module '$u'" }
        $extra += $Optional[$u]
    }
    $sources = ($CoreC + $extra + $Asm + $src) -join ' '
    # extra compiler flags for this test (tests/expected/<name>.flags): a build that sets a compile-time option
    $flagsFile = "tests/expected/$name.flags"
    $testFlags = if (Test-Path $flagsFile) { (Get-Content $flagsFile -Raw).Trim() } else { '' }
    $programFlags = Get-ProgramFlags $name
    if ($testFlags -ne '' -and $programFlags -ne '') { throw "$name has both a .flags and a .cflags file" }
    $expectedPath = "tests/expected/$name.expected"
    $reference = $null
    # A library option means a library built with it; a program-only option means the archive, always.
    $fromSource = ($FromSources -or ($testFlags -ne '')) -and ($programFlags -eq '')

    foreach ($level in $LevelList) {
        $label = "{0,-22} -O{1}" -f $name, $level
        $out = "build/$name.O$level.out"
        $err = "build/$name.O$level.err"
        $body = if ($fromSource) { "$sources $testFlags" } else { "$src $(Get-LibraryArgs $level $use) $programFlags" }
        $transcript = "build/$name.O$level.transcript"
        $screenLog = if (Test-UsesTextPlane $src) { "build/$name.O$level.screen" } else { '' }
        $stdin = "tests/expected/$name.stdin"        # what is typed on the program's terminal, if anything
        if (-not (Test-Path $stdin)) { $stdin = '' }
        Remove-Item $transcript -Force -ErrorAction SilentlyContinue
        if ($screenLog) { Remove-Item $screenLog -Force -ErrorAction SilentlyContinue }
        # The run's own words first: a .run file may end with `-- a b`, the program's arguments.
        $cmdLine = "$body -I include -O$level -Werror -o build/$name.O$level.cres $LinkFlags --run --clean --ceres-path `"$CeresDir`" $(Get-RunArgs $transcript $screenLog $stdin)"
        # tests/expected/<name>.ports: media to plug in, one `--port 0=file` or `--cart 1=file` per line. The files a
        # test writes to are new for every level, so each run starts from the same empty stick.
        $portsFile = "tests/expected/$name.ports"
        if (Test-Path $portsFile) {
            New-Item -ItemType Directory -Force "build/ports" | Out-Null
            Remove-Item "build/ports/*" -Force -ErrorAction SilentlyContinue
            foreach ($spec in (Get-Content $portsFile | Where-Object { $_.Trim() })) {
                $pair = $spec.Trim() -split '\s+', 2
                $cmdLine += " --run-arg $($pair[0]) --run-arg $($pair[1])"
            }
        }
        # tests/expected/<name>.run: more for `ceres run`, one or more words a line - `--env NAME=value`, and last
        # `-- a b` for the program's own arguments.
        $runFile = "tests/expected/$name.run"
        if (Test-Path $runFile) {
            foreach ($word in ((Get-Content $runFile) -join ' ').Trim() -split '\s+') {
                if ($word) { $cmdLine += " --run-arg $word" }
            }
        }
        if ((Test-Path $runFile) -and ((Get-Content $runFile -Raw) -match '--host-dir\s+build/host(\s|$)')) {
            # a new, empty host directory for every level, as for the sticks
            Remove-Item "build/host" -Recurse -Force -ErrorAction SilentlyContinue
            New-Item -ItemType Directory -Force "build/host" | Out-Null
            if (Test-Path "tests/data/host") { Get-ChildItem "tests/data/host" -Force | Copy-Item -Destination "build/host" -Recurse -Force }
        }
        $code = Invoke-Tool $Ceresc $cmdLine $out $err
        $errText = Read-Text $err
        $statusFile = "tests/expected/$name.status"          # the exit status the test must end with (default 0)
        $wantStatus = if (Test-Path $statusFile) { [int]((Read-Text $statusFile).Trim()) } else { 0 }

        if ($code -ne $wantStatus) {
            [void]$failures.Add("$name -O$level (exit $code)")
            Write-Host "  FAIL  $label  the build or the run failed (exit $code)" -ForegroundColor Red
            ($errText -split "`r?`n") | Where-Object { $_ -and $_ -notmatch '^Wrote ' } | Select-Object -First 6 | ForEach-Object { Write-Host "      $_" -ForegroundColor DarkYellow }
            continue
        }

        $leaked = Get-HostOutput $out
        if (-not (Same $leaked '')) {
            [void]$failures.Add("$name -O$level (host stdout)")
            Write-Host "  FAIL  $label  wrote to the host's stdout" -ForegroundColor Red
            Show-Difference '' $leaked
            continue
        }

        $raw = Read-Text $transcript
        $streams = Split-Transcript $(if ($null -eq $raw) { '' } else { $raw })
        $actual = $streams.Out

        if ($Update -and $level -eq $LevelList[0]) {
            [System.IO.File]::WriteAllBytes("$Root\$expectedPath", $Latin1.GetBytes($actual))
            Write-Host "  wrote $expectedPath ($($actual.Length) bytes)" -ForegroundColor Yellow
        }
        if ($null -eq $reference) { $reference = $actual }
        elseif (-not (Same $actual $reference)) {
            [void]$failures.Add("$name -O$level (differs from -O$($LevelList[0]))")
            Write-Host "  FAIL  $label  prints something different from -O$($LevelList[0])" -ForegroundColor Red
            Show-Difference $reference $actual
            continue
        }

        # What the program wrote to its error stream, then what the run itself said on stderr (ceresc's "Wrote" lines
        # and the assembler's notes on an optimized unit - "  warning [x.casm:n] ... never used" - left out):
        # tests/expected/<name>.stderr, or nothing at all.
        $errActual = $streams.Err + (((Get-ProgramOutput $errText) -split "(?<=`n)" | Where-Object { $_ -notmatch '^(Wrote |  warning \[)' }) -join '')
        $errExpectedPath = "tests/expected/$name.stderr"
        $errExpected = Read-Text $errExpectedPath
        $errExpected = if ($null -eq $errExpected) { '' } else { $errExpected -replace "`r`n", "`n" }
        if ($Update -and $level -eq $LevelList[0]) {
            # the file follows what the test writes now: gone when it writes nothing
            if (Same $errActual '') { Remove-Item $errExpectedPath -Force -ErrorAction SilentlyContinue }
            else { [System.IO.File]::WriteAllBytes("$Root\$errExpectedPath", $Latin1.GetBytes($errActual)) }
            $errExpected = $errActual
        }
        if (-not (Same $errExpected $errActual)) {
            [void]$failures.Add("$name -O$level (stderr)")
            Write-Host "  FAIL  $label  its error stream differs from $errExpectedPath" -ForegroundColor Red
            Show-Difference $errExpected $errActual
            continue
        }

        if ($screenLog) {
            $screenPath = "tests/expected/$name.screen"
            $screenRaw = Read-Text $screenLog
            $screenActual = if ($null -eq $screenRaw) { '' } else { $screenRaw -replace "`r`n", "`n" }
            if ($Update -and $level -eq $LevelList[0]) {
                [System.IO.File]::WriteAllBytes("$Root\$screenPath", $Latin1.GetBytes($screenActual))
            }
            $screenExpected = Read-Text $screenPath
            $screenExpected = if ($null -eq $screenExpected) { '' } else { $screenExpected -replace "`r`n", "`n" }
            if (-not (Same $screenExpected $screenActual)) {
                [void]$failures.Add("$name -O$level (screen)")
                Write-Host "  FAIL  $label  its screens differ from $screenPath" -ForegroundColor Red
                Show-Difference $screenExpected $screenActual
                continue
            }
        }

        $expected = Read-Text $expectedPath
        if ($null -eq $expected) {
            [void]$failures.Add("$name (no $expectedPath)")
            Write-Host "  FAIL  $label  there is no $expectedPath (run with -Update, then review it)" -ForegroundColor Red
        }
        elseif (-not (Same ($expected -replace "`r`n", "`n") $actual)) {
            [void]$failures.Add("$name -O$level (output)")
            Write-Host "  FAIL  $label  output differs from the expected file" -ForegroundColor Red
            Show-Difference ($expected -replace "`r`n", "`n") $actual
        }
        else {
            $passed++
            Write-Host "  ok    $label" -ForegroundColor Green
        }
    }
}

Test-EachHeader
Test-Img2tiles
Test-Examples
if (-not $archivesBuilt) { Ensure-Library @(2) }   # the shell is always linked against the archive
Test-Shell

Write-Host ""
if ($failures.Count -eq 0) {
    Write-Host "all tests passed ($passed checks: $($tests.Count) tests x $($LevelList.Count) levels, plus the headers)" -ForegroundColor Green
    exit 0
}
Write-Host "$($failures.Count) failure(s):" -ForegroundColor Red
$failures | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
exit 1
