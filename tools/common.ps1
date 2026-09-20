# Shared by mklib.ps1 and runtests.ps1: where the tools are, which sources make up the library, and how
# to run a native program with its stdout captured byte for byte. Dot-source it:  . "$PSScriptRoot\common.ps1"

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root
$Latin1 = [System.Text.Encoding]::GetEncoding(28591)   # one char per byte: NULs survive
if (-not $TimeoutSeconds) { $TimeoutSeconds = 180 }

# ---- the tools ----------------------------------------------------------------------------------
# Found next to this checkout (../../Ceres-C, ../../CeresASM), or through CERESC and CERES_DIR.

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

if ($env:CERES_DIR -and (Test-Path $env:CERES_DIR)) { $CeresDir = (Resolve-Path $env:CERES_DIR).Path }
elseif (Test-Path "$Root\..\..\CeresASM\ceres.exe") { $CeresDir = (Resolve-Path "$Root\..\..\CeresASM").Path }
else { $CeresDir = Split-Path -Parent (Find-Tool $null @() 'ceres') }
$Ceres = Join-Path $CeresDir 'ceres.exe'

# ---- the sources --------------------------------------------------------------------------------

# Modules that bind interrupt vectors are opt-in: the linker allows ONE `interrupt N` binding per
# number in a whole program, so a program that binds its own must not carry them.
$Optional = @{ irq = 'src/ceres/irq.c' }

function Get-Rel([string]$full) { return $full.Substring($Root.Length + 1).Replace('\', '/') }

$AllC = @(Get-ChildItem src -Recurse -Filter *.c | ForEach-Object { Get-Rel $_.FullName } | Sort-Object)
$CoreC = @($AllC | Where-Object { $Optional.Values -notcontains $_ })
$Asm = @(Get-ChildItem asm -Filter *.casm -ErrorAction SilentlyContinue | ForEach-Object { Get-Rel $_.FullName } | Sort-Object)

New-Item -ItemType Directory -Force build | Out-Null

# ---- running a native tool with its stdout in a file (bytes preserved) -----------------------------

function Invoke-Tool([string]$exe, [string]$argLine, [string]$outFile, [string]$errFile) {
    $p = Start-Process -FilePath $exe -ArgumentList $argLine -WorkingDirectory $Root -NoNewWindow -PassThru `
        -RedirectStandardOutput $outFile -RedirectStandardError $errFile
    $null = $p.Handle                         # without this ExitCode can come back empty
    if (-not $p.WaitForExit($TimeoutSeconds * 1000)) {
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
