# Shared by mklib.ps1 and runtests.ps1: where the tools are, which sources make up the library, and how
# to run a native program with its stdout captured byte for byte. Dot-source it:  . "$PSScriptRoot\common.ps1"

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root
$Latin1 = [System.Text.Encoding]::GetEncoding(28591)   # one char per byte: NULs survive
if (-not $TimeoutSeconds) { $TimeoutSeconds = 180 }

# ---- the tools ----------------------------------------------------------------------------------
# Found the way ceresc finds ceres (--ceres-path, CERES_PATH, then PATH): the environment variable named after the tool
# (CERES, CERESC: the executable, or the directory that holds it), else the directory CERES_PATH names (where Ceres is
# installed; a file in it stands for the directory), else PATH. A variable that is set decides: naming no such tool is
# an error, not a reason to look elsewhere. Nothing is looked for next to this checkout.

function Find-Tool([string]$name, [string]$variable) {
    $own = [Environment]::GetEnvironmentVariable($variable)
    $installed = $env:CERES_PATH
    if ($own -or $installed) {
        $from = if ($own) { $variable } else { 'CERES_PATH' }
        $given = if ($own) { $own } else { $installed }
        if (-not $own -and (Test-Path -LiteralPath $given -PathType Leaf)) { $given = Split-Path -Parent $given }
        $candidate = if (Test-Path -LiteralPath $given -PathType Container) { Join-Path $given "$name.exe" } elseif ($own) { $given } else { '' }
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) { return (Resolve-Path -LiteralPath $candidate).Path }
        throw "$from names '$(if ($own) { $own } else { $installed })', which holds no $name (unset it to look on PATH)"
    }
    $cmd = Get-Command "$name.exe" -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($cmd) { return $cmd.Source }
    throw "cannot find $name - set CERES_PATH to the directory Ceres is installed in (or $variable to it), or put its directory on PATH"
}

$Ceresc = Find-Tool 'ceresc' 'CERESC'
$Ceres = Find-Tool 'ceres' 'CERES'
$CeresDir = Split-Path -Parent $Ceres

# ---- the sources --------------------------------------------------------------------------------

# Modules that bind interrupt vectors are opt-in: the linker allows ONE `interrupt N` binding per
# number in a whole program, so a program that binds its own must not carry them.
# Each module is a list of files: a C file and, for fault, the assembly that binds its vectors.
$Optional = @{
    irq   = @('src/ceres/irq.c')
    fault = @('src/ceres/fault.c', 'asm/optional/fault.casm')
    mmu   = @('src/ceres/mmu_fault.c', 'asm/optional/mmu_fault.casm')
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

# A program of one C file built against the archive at a level, the way a program is linked without --run: compiled to
# CASM against the archive's declarations, assembled, and linked with the archive ($work names the files on the way).
# $null when it built; otherwise what went wrong. $defines: more compiler flags (-DSHELL_SMALL).
function Build-Program([string]$source, [string]$cres, [string]$work, [int]$level = 2, [string]$linkFlags = '', [string]$defines = '') {
    $dir = Get-LibraryDir $level
    $steps = @(
        @($Ceresc, "$source $defines --decls $dir/libceres.decls.casm -I include -O$level -Werror -S -o $work.casm"),
        @($Ceres, "asm -c $work.casm -o $work.cobj"),
        @($Ceres, "link $work.cobj $dir/libceres.car -o $cres $linkFlags"))
    foreach ($step in $steps) {
        $code = Invoke-Tool $step[0] $step[1] "$work.out" "$work.err"
        if ($code -ne 0) { return "$([System.IO.Path]::GetFileName($step[0])): $(([string](Read-Text "$work.err")).Trim())" }
    }
    return $null
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
