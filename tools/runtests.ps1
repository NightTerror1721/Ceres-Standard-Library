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

    A test with a tests/expected/<name>.flags file sets a compile-time option of the LIBRARY (-DCERES_...), so
    the library is compiled again with it, together with the test, as before. -FromSources does that for every
    test: the slow path, and the one that proves the archive changes nothing.

    The tools are found next to this checkout (../../Ceres-C, ../../CeresASM) or through the
    CERESC and CERES_DIR environment variables.

.EXAMPLE
    tools\runtests.ps1                      # everything
    tools\runtests.ps1 -Test test_malloc    # one test
    tools\runtests.ps1 -Headers             # only "each header compiles on its own"
    tools\runtests.ps1 -Update              # write tests/expected from the -O0 output (review it!)
    tools\runtests.ps1 -FromSources         # compile the whole library into every test, as it once was
#>
[CmdletBinding()]
param(
    [string[]]$Test = @(),
    [string]$Levels = "0,1,2",          # optimization levels, e.g. -Levels 0,2
    [switch]$Headers,
    [switch]$Update,
    [switch]$FromSources,
    [int]$TimeoutSeconds = 180
)

# `powershell -File x.ps1 -Test a,b` delivers "a,b" as ONE string: split it here.
$Test = @($Test | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$LevelList = @(($Levels -join ',') -split '[,; ]+' | Where-Object { $_ } | ForEach-Object { [int]$_ })

. "$PSScriptRoot/common.ps1"    # $Root, $Ceresc, $CeresDir, the source lists, Invoke-Tool, Same, ...

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
        $statusFile = "examples/expected/$name.status"
        $wantStatus = if (Test-Path $statusFile) { [int]((Read-Text $statusFile).Trim()) } else { 0 }
        if (Test-Path $expectedPath) {
            # ceresc builds, links and runs in one go; the program reads its stdin from the .stdin file
            $body = if ($FromSources) { "$sources $flags" } else { "examples/$name.c $(Get-LibraryArgs 2 $use) $flags" }   # a define only the example reads goes with either
            $cmd = "$body -I include -O2 -Werror -o build/examples/$name.cres --run --clean --ceres-path `"$CeresDir`""
            $code = Invoke-Tool $Ceresc $cmd "build/examples/$name.out" "build/examples/$name.err" $stdin
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
        $actual = Get-ProgramOutput (Read-Text "build/examples/$name.out")
        if ($Update) {
            [System.IO.File]::WriteAllBytes("$Root\$expectedPath", $Latin1.GetBytes($actual))
            Write-Host "  wrote $expectedPath ($($actual.Length) bytes)" -ForegroundColor Yellow
        }
        elseif (-not (Same $actual ((Read-Text $expectedPath) -replace "`r`n", "`n"))) {
            $bad++
            [void]$failures.Add("example $name (output)")
            Write-Host "  FAIL  $name  output differs from $expectedPath" -ForegroundColor Red
            Show-Difference ((Read-Text $expectedPath) -replace "`r`n", "`n") $actual
        }
    }
    if ($bad -eq 0) {
        Write-Host "  ok    $count examples" -ForegroundColor Green
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

if (-not $FromSources) { Ensure-Library @($LevelList + 2 | Sort-Object -Unique) }   # -O2 also serves the examples

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
    $expectedPath = "tests/expected/$name.expected"
    $reference = $null
    $fromSource = $FromSources -or ($testFlags -ne '')     # a library option means a library built with it

    foreach ($level in $LevelList) {
        $label = "{0,-22} -O{1}" -f $name, $level
        $out = "build/$name.O$level.out"
        $err = "build/$name.O$level.err"
        $body = if ($fromSource) { "$sources $testFlags" } else { "$src $(Get-LibraryArgs $level $use)" }
        $cmdLine = "$body -I include -O$level -Werror -o build/$name.O$level.cres --run --clean --ceres-path `"$CeresDir`""
        $stdin = "tests/expected/$name.stdin"        # what the program reads from the terminal, if it reads
        if (-not (Test-Path $stdin)) { $stdin = '' }
        $code = Invoke-Tool $Ceresc $cmdLine $out $err $stdin
        $errText = Read-Text $err
        $statusFile = "tests/expected/$name.status"          # the exit status the test must end with (default 0)
        $wantStatus = if (Test-Path $statusFile) { [int]((Read-Text $statusFile).Trim()) } else { 0 }

        if ($code -ne $wantStatus) {
            [void]$failures.Add("$name -O$level (exit $code)")
            Write-Host "  FAIL  $label  the build or the run failed (exit $code)" -ForegroundColor Red
            ($errText -split "`r?`n") | Where-Object { $_ -and $_ -notmatch '^Wrote ' } | Select-Object -First 6 | ForEach-Object { Write-Host "      $_" -ForegroundColor DarkYellow }
            continue
        }

        $actual = Get-ProgramOutput (Read-Text $out)

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
Test-Examples

Write-Host ""
if ($failures.Count -eq 0) {
    Write-Host "all tests passed ($passed checks: $($tests.Count) tests x $($LevelList.Count) levels, plus the headers)" -ForegroundColor Green
    exit 0
}
Write-Host "$($failures.Count) failure(s):" -ForegroundColor Red
$failures | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
exit 1
