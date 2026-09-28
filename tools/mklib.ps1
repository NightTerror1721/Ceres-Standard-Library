<#
.SYNOPSIS
    Builds the library once per optimization level: every source compiled, ASSEMBLED, and collected into an
    archive, with the declarations file that names what it defines.

.DESCRIPTION
    For each level in -Levels (default 0,1,2) this writes build/lib/O<level>/:

        libceres.car          the archive
        libceres.decls.casm   the declarations of everything in it (what ceresc's --decls takes)
        obj/                  the objects, one per unit

    A program is then built against it with ceresc alone, and nothing of the library is compiled again:

        ceresc prog.c build/lib/O2/libceres.car --decls build/lib/O2/libceres.decls.casm -I include -O2 --run

    The steps for one level:
    1. ceresc turns every src/**/*.c into a .casm and writes libceres.decls.casm (the declarations file each
       generated unit imports to name the others' symbols).
    2. `ceres asm -c` assembles each of those and each hand-written asm/*.casm into an object. An error only
       the assembler can see shows up here, not at the first test.
    3. `ceres ar` collects the objects. Objects named on a link line are part of the program whether or not
       anything calls them, but ARCHIVE MEMBERS ARE PULLED IN ONLY WHEN THEY ANSWER A NAME NOTHING ELSE
       DEFINES - so a program that calls one routine carries one, and the modules that bind interrupt vectors
       (src/ceres/irq.c) stay out unless something calls into them.
    4. Verification (-O2 only, unless -NoVerify): three programs are built against the archive with ceresc and
       run. One of them binds vector 19 itself; it can only link because irq.cobj, which also binds 19, was
       never pulled. A program writes to the machine's terminal, never to the host's stdout (CeresASM plan/v2
       F5), so each runs headless and its output stream is read back from `ceres run --transcript`, then
       compared with tests/expected/<name>.expected; it must exit with 0, or with tests/expected/<name>.status.

    build/libceres.car and build/libceres.decls.casm are kept as copies of the -O2 ones, where they have
    always been.

.EXAMPLE
    tools\mklib.ps1                  # all three levels, and verify
    tools\mklib.ps1 -Levels 2        # only -O2
    tools\mklib.ps1 -NoVerify        # just the archives
    tools\mklib.ps1 -Keep            # leave the generated .casm files next to the sources
#>
[CmdletBinding()]
param(
    [string]$Levels = "0,1,2",
    [switch]$Keep,
    [switch]$NoVerify,
    [int]$TimeoutSeconds = 180
)

. "$PSScriptRoot/common.ps1"

function Fail([string]$what, [string]$errFile) {
    Write-Host "FAILED: $what" -ForegroundColor Red
    if ($errFile -and (Test-Path $errFile)) {
        (Read-Text $errFile) -split "`r?`n" | Where-Object { $_ } | Select-Object -First 8 | ForEach-Object { Write-Host "  $_" -ForegroundColor DarkYellow }
    }
    exit 1
}

function Remove-Generated {
    Get-ChildItem src -Recurse -Filter *.casm -ErrorAction SilentlyContinue | Remove-Item -Force
    Get-ChildItem tests -Filter *.casm -ErrorAction SilentlyContinue | Remove-Item -Force
}

$LevelList = @(($Levels -split '[,; ]+') | Where-Object { $_ } | ForEach-Object { [int]$_ })

Write-Host "ceresc  $Ceresc" -ForegroundColor DarkGray
Write-Host "ceres   $CeresDir" -ForegroundColor DarkGray

foreach ($level in $LevelList) {
    $dir = Get-LibraryDir $level
    $objDir = "$dir/obj"
    if (Test-Path $dir) { Remove-Item $dir -Recurse -Force }
    New-Item -ItemType Directory -Force $objDir | Out-Null

    # ---- 1. C -> CASM -----------------------------------------------------------------------------
    Write-Host "-O$level : compiling $($AllC.Count) C files" -ForegroundColor Cyan
    $code = Invoke-Tool $Ceresc "$($AllC -join ' ') -I include -O$level -Werror -S -o $dir/libceres.cres" "$dir/compile.out" "$dir/compile.err"
    if ($code -ne 0) { Fail "ceresc could not compile the library at -O$level" "$dir/compile.err" }

    # ---- 2. CASM -> objects -----------------------------------------------------------------------
    $units = @($AllC | ForEach-Object { [System.IO.Path]::ChangeExtension($_, '.casm') }) + $Asm + $OptionalAsm
    Write-Host "-O$level : assembling $($units.Count) units" -ForegroundColor Cyan
    foreach ($u in $units) {
        $o = "$objDir/$(Get-FlatName $u).cobj"
        $code = Invoke-Tool $Ceres "asm -c $u -o $o" "$dir/asm.out" "$dir/asm.err"
        if ($code -ne 0) { Fail "ceres asm -c $u" "$dir/asm.err" }
    }

    # ---- 3. the archive ---------------------------------------------------------------------------
    $objects = @(Get-ChildItem $objDir -Filter *.cobj | Sort-Object Name | ForEach-Object { "$objDir/$($_.Name)" })
    $code = Invoke-Tool $Ceres "ar $dir/libceres.car $($objects -join ' ')" "$dir/ar.out" "$dir/ar.err"
    if ($code -ne 0) { Fail 'ceres ar' "$dir/ar.err" }
    $size = (Get-Item "$dir/libceres.car").Length
    Write-Host "$dir/libceres.car: $($objects.Count) objects, $size bytes" -ForegroundColor Green
}

# the -O2 archive where it has always been
if ($LevelList -contains 2) {
    Copy-Item "$(Get-LibraryDir 2)/libceres.car" build/libceres.car -Force
    Copy-Item "$(Get-LibraryDir 2)/libceres.decls.casm" build/libceres.decls.casm -Force
}

# ---- 4. build three programs against the archive with ceresc alone, and run them --------------------------

# The transcript's output stream: the error stream's bytes are between ESC [ E and ESC [ e (as in runtests.ps1).
function Get-TranscriptOutput([string]$raw) {
    $out = New-Object System.Text.StringBuilder
    $inError = $false
    for ($i = 0; $i -lt $raw.Length; $i++) {
        if ($raw[$i] -eq [char]27 -and $i + 2 -lt $raw.Length -and $raw[$i + 1] -eq '[' -and ($raw[$i + 2] -ceq 'E' -or $raw[$i + 2] -ceq 'e')) {
            $inError = $raw[$i + 2] -ceq 'E'
            $i += 2
        }
        elseif (-not $inError) { [void]$out.Append($raw[$i]) }
    }
    return $out.ToString()
}

if (-not $NoVerify -and ($LevelList -contains 2)) {
    $dir = Get-LibraryDir 2
    New-Item -ItemType Directory -Force build/verify | Out-Null
    $env:CERES_HEADLESS = '1'                   # no window, whatever ceresc starts
    $programs = @('hello', 'test_user_irq19', 'test_double')
    foreach ($name in $programs) {
        $transcript = "build/verify/$name.transcript"
        Remove-Item $transcript -Force -ErrorAction SilentlyContinue
        $runArgs = (@('--headless', '--speed', 'max', '--gpu', 'software', '--transcript', $transcript) | ForEach-Object { "--run-arg $_" }) -join ' '
        $code = Invoke-Tool $Ceresc "tests/$name.c $dir/libceres.car --decls $dir/libceres.decls.casm -I include -O2 -o build/verify/$name.cres --run --clean --ceres-path `"$CeresDir`" $runArgs" "build/verify/$name.out" build/mklib.v.err
        $statusFile = "tests/expected/$name.status"
        $wantStatus = if (Test-Path $statusFile) { [int]((Read-Text $statusFile).Trim()) } else { 0 }
        if ($code -ne $wantStatus) { Fail "building and running $name against libceres.car (exit $code, expected $wantStatus)" build/mklib.v.err }

        $raw = Read-Text $transcript
        $actual = (Get-TranscriptOutput $(if ($null -eq $raw) { '' } else { $raw })) -replace "`r`n", "`n"
        $expected = (Read-Text "tests/expected/$name.expected") -replace "`r`n", "`n"
        if (-not (Same $actual $expected)) { Fail "$name printed something other than tests/expected/$name.expected" $null }
        Write-Host ("  built against the archive and ran: {0}" -f $name) -ForegroundColor Green
    }
}

if (-not $Keep) { Remove-Generated }
Write-Host "done" -ForegroundColor Green
