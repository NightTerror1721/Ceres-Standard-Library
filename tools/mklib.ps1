<#
.SYNOPSIS
    Builds build/libceres.car: every library source compiled, ASSEMBLED, and collected into an archive.

.DESCRIPTION
    1. ceresc turns every src/**/*.c into a .casm and writes build/libceres.decls.casm (the
       declarations file each generated unit imports to name the others' symbols).
    2. `ceres asm -c` assembles each of those and each hand-written asm/*.casm into an object.
       This is the step the old Makefile never ran: an error only the assembler can see used to wait
       for the first test.
    3. `ceres ar` collects the objects. Objects named on a link line are part of the program whether
       or not anything calls them, but ARCHIVE MEMBERS ARE PULLED IN ONLY WHEN THEY ANSWER A NAME
       NOTHING ELSE DEFINES - so a program that calls one routine carries one, and the modules that
       bind interrupt vectors (src/ceres/irq.c) stay out unless something calls into them.
    4. Verification: two programs are linked against the archive and run. One of them binds vector 17
       itself; it can only link because irq.cobj, which also binds 17, was never pulled.

    ceresc cannot take a .car itself yet (it links every object it is given), so a program is built
    against the archive by hand, as step 4 shows: compile with the declarations file, `ceres asm -c`,
    then `ceres link main.cobj build/libceres.car`.

.EXAMPLE
    tools\mklib.ps1              # build and verify
    tools\mklib.ps1 -NoVerify    # just the archive
    tools\mklib.ps1 -Keep        # leave the generated .casm files next to the sources
#>
[CmdletBinding()]
param(
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

function Flat([string]$path) { return ($path -replace '\.[^./\\]+$', '') -replace '[/\\.]', '_' }

Write-Host "ceresc  $Ceresc" -ForegroundColor DarkGray
Write-Host "ceres   $CeresDir" -ForegroundColor DarkGray

$objDir = 'build/obj'
if (Test-Path $objDir) { Remove-Item $objDir -Recurse -Force }
New-Item -ItemType Directory -Force $objDir | Out-Null
if (Test-Path build/libceres.car) { Remove-Item build/libceres.car -Force }

# ---- 1. C -> CASM ---------------------------------------------------------------------------------

Write-Host "compiling $($AllC.Count) C files" -ForegroundColor Cyan
$code = Invoke-Tool $Ceresc "$($AllC -join ' ') -I include -O2 -Werror -S -o build/libceres.cres" build/mklib.compile.out build/mklib.compile.err
if ($code -ne 0) { Fail 'ceresc could not compile the library' build/mklib.compile.err }

# ---- 2. CASM -> objects ---------------------------------------------------------------------------

$units = @($AllC | ForEach-Object { [System.IO.Path]::ChangeExtension($_, '.casm') }) + $Asm + $OptionalAsm
Write-Host "assembling $($units.Count) units" -ForegroundColor Cyan
foreach ($u in $units) {
    $o = "$objDir/$(Flat $u).cobj"
    $code = Invoke-Tool $Ceres "asm -c $u -o $o" build/mklib.asm.out build/mklib.asm.err
    if ($code -ne 0) { Fail "ceres asm -c $u" build/mklib.asm.err }
}

# ---- 3. the archive -------------------------------------------------------------------------------

$objects = @(Get-ChildItem $objDir -Filter *.cobj | Sort-Object Name | ForEach-Object { "$objDir/$($_.Name)" })
$code = Invoke-Tool $Ceres "ar build/libceres.car $($objects -join ' ')" build/mklib.ar.out build/mklib.ar.err
if ($code -ne 0) { Fail 'ceres ar' build/mklib.ar.err }
$size = (Get-Item build/libceres.car).Length
Write-Host "build/libceres.car: $($objects.Count) objects, $size bytes" -ForegroundColor Green

# ---- 4. link two programs against it ---------------------------------------------------------------

if (-not $NoVerify) {
    New-Item -ItemType Directory -Force build/verify | Out-Null
    foreach ($name in @('hello', 'test_user_irq17')) {
        $src = "tests/$name.c"
        # Compiled TOGETHER with the library only to get the declarations file that names the library's
        # symbols; just the program's own .casm is assembled, and the library comes from the archive.
        $code = Invoke-Tool $Ceresc "$src $($AllC -join ' ') -I include -O2 -S -o build/verify/$name.cres" build/mklib.v.out build/mklib.v.err
        if ($code -ne 0) { Fail "ceresc $src" build/mklib.v.err }
        $code = Invoke-Tool $Ceres "asm -c tests/$name.casm -o build/verify/$name.cobj" build/mklib.v.out build/mklib.v.err
        if ($code -ne 0) { Fail "ceres asm -c tests/$name.casm" build/mklib.v.err }
        $code = Invoke-Tool $Ceres "link build/verify/$name.cobj build/libceres.car -o build/verify/$name.cres" build/mklib.v.out build/mklib.v.err
        if ($code -ne 0) { Fail "ceres link $name against libceres.car" build/mklib.v.err }
        $code = Invoke-Tool $Ceres "run build/verify/$name.cres" "build/verify/$name.out" build/mklib.v.err
        if ($code -ne 0) { Fail "running $name" build/mklib.v.err }

        $actual = (Read-Text "build/verify/$name.out") -replace "`r`n", "`n"
        $expected = (Read-Text "tests/expected/$name.expected") -replace "`r`n", "`n"
        if (-not (Same $actual $expected)) { Fail "$name printed something other than tests/expected/$name.expected" $null }
        $bytes = (Get-Item "build/verify/$name.cres").Length
        Write-Host ("  linked against the archive and ran: {0,-18} {1} bytes" -f $name, $bytes) -ForegroundColor Green
    }
}

if (-not $Keep) { Remove-Generated }
Write-Host "done" -ForegroundColor Green
