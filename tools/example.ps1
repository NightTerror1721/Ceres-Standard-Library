<#
.SYNOPSIS
    Builds one program from examples/ against the library and runs it.

.DESCRIPTION
    Compiles examples/<Name>.c at -O2 (-Level) and links it against the library built at that level,
    build/lib/O<level>/libceres.car (rebuilt by tools/mklib.ps1 when a library source is newer than it): only
    the modules the program calls are linked, the optional ones (irq, fault) among them. Nothing of the library
    is compiled again; ceresc gets the archive's declarations with --decls.

    A machine has a screen: the program runs in its window, where its terminal and its graphics are, and the
    keyboard, mouse and gamepad go to it; the window stays with the last frame when it ends, until a key is pressed.
    -Headless runs it without one (ceres run --headless): what it writes to its terminal is then shown here when it
    ends (--transcript), and -Type <file> types a file on its terminal (--type).

.EXAMPLE
    tools\example.ps1 snake                            # in the window
    tools\example.ps1 calc -Headless -Type input.txt   # no window: its output here
    tools\example.ps1 life -Define DEMO_FRAMES=100      # the demo build the tests compare
    tools\example.ps1 hello
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)][string]$Name,
    [switch]$Headless,
    [string]$Type = '',
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
$transcript = "build/examples/$Name.transcript"
if ($Headless) { $runArgs += @("--headless", "--transcript", $transcript) }
if ($Type) { $runArgs += @("--type", $Type) }
& $Ceres @runArgs
$code = $LASTEXITCODE
if ($Headless -and (Test-Path $transcript)) { Write-Host ([System.IO.File]::ReadAllText("$Root\$transcript")) }
exit $code
