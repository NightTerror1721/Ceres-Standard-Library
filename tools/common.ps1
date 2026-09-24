# Shared by mklib.ps1 and runtests.ps1: where the tools are, which sources make up the library, and how
# to run a native program with its stdout captured byte for byte. Dot-source it:  . "$PSScriptRoot\common.ps1"

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root
$Latin1 = [System.Text.Encoding]::GetEncoding(28591)   # one char per byte: NULs survive
if (-not $TimeoutSeconds) { $TimeoutSeconds = 180 }

# ---- the tools ----------------------------------------------------------------------------------
# Found next to this checkout (../../Ceres-C, ../../CeresASM), or through CERESC and CERES_PATH.
# CERES_PATH is the standard way to say where ceres is (the directory that holds it, or the executable itself);
# CERES_DIR, its older name here, still works.

function Find-Tool([string]$fromEnv, [string[]]$candidates, [string]$onPath) {
    if ($fromEnv -and (Test-Path $fromEnv)) { return (Resolve-Path $fromEnv).Path }
    foreach ($c in $candidates) { if (Test-Path $c) { return (Resolve-Path $c).Path } }
    $cmd = Get-Command $onPath -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    throw "cannot find ${onPath} - set the environment variable or build it next to this checkout"
}

$Ceresc = Find-Tool $env:CERESC @(
    "$Root\..\..\Ceres-C\build\gcc\bin\Release\ceresc.exe",
    "$Root\..\..\Ceres-C\build\ninja\bin\Release\ceresc.exe",
    "$Root\..\..\Ceres-C\build\msvc\bin\Release\ceresc.exe") 'ceresc'

function Resolve-CeresLocation([string]$where) {
    if (-not $where -or -not (Test-Path $where)) { return $null }
    $item = Get-Item $where
    if ($item.PSIsContainer) { return $item.FullName }
    return $item.DirectoryName
}
$fromEnv = Resolve-CeresLocation $env:CERES_PATH
if (-not $fromEnv) { $fromEnv = Resolve-CeresLocation $env:CERES_DIR }
if ($fromEnv) { $CeresDir = $fromEnv }
elseif (Test-Path "$Root\..\..\CeresASM\ceres.exe") { $CeresDir = (Resolve-Path "$Root\..\..\CeresASM").Path }
else { $CeresDir = Split-Path -Parent (Find-Tool $null @() 'ceres') }
$Ceres = Join-Path $CeresDir 'ceres.exe'

# ---- the sources --------------------------------------------------------------------------------

# Modules that bind interrupt vectors are opt-in: the linker allows ONE `interrupt N` binding per
# number in a whole program, so a program that binds its own must not carry them.
# Each module is a list of files: a C file and, for fault, the assembly that binds its vectors.
$Optional = @{
    irq   = @('src/ceres/irq.c')
    fault = @('src/ceres/fault.c', 'asm/optional/fault.casm')
}
$OptionalFiles = @($Optional.Values | ForEach-Object { $_ })
$OptionalAsm = @($OptionalFiles | Where-Object { $_ -like '*.casm' })

function Get-Rel([string]$full) { return $full.Substring($Root.Length + 1).Replace('\', '/') }

$AllC = @(Get-ChildItem src -Recurse -Filter *.c | ForEach-Object { Get-Rel $_.FullName } | Sort-Object)
$CoreC = @($AllC | Where-Object { $OptionalFiles -notcontains $_ })
$Asm = @(Get-ChildItem asm -Filter *.casm -ErrorAction SilentlyContinue | ForEach-Object { Get-Rel $_.FullName } | Sort-Object)

New-Item -ItemType Directory -Force build | Out-Null

# ---- the library, built once per optimization level (tools/mklib.ps1) --------------------------------------

function Get-LibraryDir([int]$level) { return "build/lib/O$level" }

# The name an object gets in the archive's obj/ directory: the unit's path without its extension, flattened.
function Get-FlatName([string]$path) { return ($path -replace '\.[^./\\]+$', '') -replace '[/\\.]', '_' }

# True when build/lib/O<level> holds an archive newer than everything it is made of: the sources, the headers,
# the build script and the compiler that made it.
function Test-LibraryFresh([int]$level) {
    $archive = "$(Get-LibraryDir $level)/libceres.car"
    if (-not (Test-Path $archive) -or -not (Test-Path "$(Get-LibraryDir $level)/libceres.decls.casm")) { return $false }
    $built = (Get-Item $archive).LastWriteTime
    $newest = Get-ChildItem src, asm, include, tools/mklib.ps1 -Recurse -File | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($newest.LastWriteTime -gt $built) { return $false }
    if ((Get-Item $Ceresc).LastWriteTime -gt $built) { return $false }
    return $true
}

# Builds the archives that are missing or out of date, for the levels asked for.
function Ensure-Library([int[]]$levels) {
    $stale = @($levels | Where-Object { -not (Test-LibraryFresh $_) })
    if ($stale.Count -eq 0) { return }
    Write-Host "building the library at -O$($stale -join ', -O')" -ForegroundColor Cyan
    & "$PSScriptRoot/mklib.ps1" -Levels ($stale -join ',') -NoVerify
    if (-not $?) { throw "tools/mklib.ps1 failed" }
}

# What to put on a ceresc command line to build against the library at a level: the objects of the optional
# modules the program asked for (a `// USE: irq` line - they bind interrupt vectors, so a program gets them only
# by asking), then the archive, then the declarations.
function Get-LibraryArgs([int]$level, [string[]]$modules) {
    $dir = Get-LibraryDir $level
    $objects = @()
    foreach ($m in $modules) {
        if (-not $Optional.ContainsKey($m)) { throw "unknown module '$m'" }
        foreach ($file in $Optional[$m]) { $objects += "$dir/obj/$(Get-FlatName $file).cobj" }
    }
    return (($objects + @("$dir/libceres.car", "--decls", "$dir/libceres.decls.casm")) -join ' ')
}

# ---- running a native tool with its stdout in a file (bytes preserved) -----------------------------

function Invoke-Tool([string]$exe, [string]$argLine, [string]$outFile, [string]$errFile, [string]$inFile = '') {
    $extra = @{}
    if ($inFile) { $extra['RedirectStandardInput'] = $inFile }      # the program's stdin comes from a file
    $p = Start-Process -FilePath $exe -ArgumentList $argLine -WorkingDirectory $Root -NoNewWindow -PassThru `
        -RedirectStandardOutput $outFile -RedirectStandardError $errFile @extra
    $null = $p.Handle                         # without this ExitCode can come back empty
    if (-not $p.WaitForExit($TimeoutSeconds * 1000)) {
        # ceresc starts `ceres` as a child: killing only ceresc would leave a hung VM holding the output files
        try { & taskkill /T /F /PID $p.Id 2>$null | Out-Null } catch { }
        try { $p.Kill() } catch { }
        return -1
    }
    return $p.ExitCode
}

function Read-Text([string]$path) {
    if (-not (Test-Path $path)) { return $null }
    return $Latin1.GetString([System.IO.File]::ReadAllBytes($path))
}

# What the program itself printed: drop the driver's "Wrote x" lines, normalize the newline.
function Get-ProgramOutput([string]$raw) {
    $text = $raw -replace '^(Wrote [^\r\n]*\r?\n)+', ''
    return $text -replace "`r`n", "`n"
}

# Byte-for-byte equality. PowerShell's -eq/-ne on strings is a CULTURE-aware, case-insensitive comparison
# that treats NUL as ignorable, so "A" -eq "A<NUL><NUL><NUL>" is TRUE - which would make the test for a
# word store into a byte register (A, 0, 0, 0) pass. Always compare through this.
function Same([string]$a, [string]$b) { return [string]::Equals($a, $b, [System.StringComparison]::Ordinal) }
