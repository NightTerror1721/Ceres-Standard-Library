<#
.SYNOPSIS
    Builds one program from examples/ against the library and runs it.

.DESCRIPTION
    Compiles examples/<Name>.c at -O2 (-Level) and links it against the library built at that level,
    build/lib/O<level>/libceres.car (rebuilt by tools/mklib.ps1 when a library source is newer than it): only
    the modules the program calls are linked, the optional ones (irq, fault) among them. Nothing of the library
    is compiled again; ceresc gets the archive's declarations with --decls. With -Window the program opens the SDL window (ceres run --window), which
    is how the games are played: the keyboard, mouse and gamepad go to the program and the display shows in
    the window. Without it the program runs headless and its terminal output appears here; the games then
    take their input from the terminal.

.EXAMPLE
    tools\example.ps1 snake -Window
    tools\example.ps1 life -Define DEMO_FRAMES=100      # the demo build the tests compare
    tools\example.ps1 hello
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)][string]$Name,
    [switch]$Window,
    [string[]]$Define = @(),
    [int]$Level = 2,
    [switch]$NoRun,
    [int]$TimeoutSeconds = 180
)

. "$PSScriptRoot/common.ps1"

$src = "examples/$Name.c"
if (-not (Test-Path $src)) { throw "no such example: $src" }

Ensure-Library @($Level)
$lib = Get-LibraryDir $Level
$archive = "$lib/libceres.car"

New-Item -ItemType Directory -Force build/examples | Out-Null
$cres = "build/examples/$Name.cres"
$defs = @()
foreach ($d in $Define) { $defs += "-D"; $defs += $d }

# Only the example is compiled: --decls names what the archive defines, and the rest comes from it at link time.
$own = "build/examples/$Name.casm"
$argList = @($src) + $defs + @("--decls", "$lib/libceres.decls.casm", "-I", "include", "-O$Level", "-Werror", "-S", "-o", $own)
$code = Invoke-Tool $Ceresc ($argList -join " ") "build/examples/$Name.compile.out" "build/examples/$Name.compile.err"
if ($code -ne 0) {
    Get-Content "build/examples/$Name.compile.err" | Select-Object -First 8 | ForEach-Object { Write-Host $_ -ForegroundColor Red }
    exit 1
}
& $Ceres asm -c $own -o "build/examples/$Name.cobj"
$code = $LASTEXITCODE
Remove-Item $own -ErrorAction SilentlyContinue
if ($code -ne 0) { exit $code }
& $Ceres link "build/examples/$Name.cobj" $archive -o $cres
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host ("built $cres ({0} bytes)" -f (Get-Item $cres).Length) -ForegroundColor Green
if ($NoRun) { exit 0 }

$runArgs = @("run", $cres)
if ($Window) { $runArgs += "--window" }
& $Ceres @runArgs
exit $LASTEXITCODE
