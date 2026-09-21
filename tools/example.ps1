<#
.SYNOPSIS
    Builds one program from examples/ against the library and runs it.

.DESCRIPTION
    Compiles examples/<Name>.c at -O2 and links it against build/libceres.car (rebuilt by tools/mklib.ps1
    when a library source is newer than it): only the modules the program calls are linked, the optional
    ones (irq, fault) among them. With -Window the program opens the SDL window (ceres run --window), which
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

$archive = "build/libceres.car"
$newest = Get-ChildItem src, asm, include, tools/mklib.ps1 -Recurse -File | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not (Test-Path $archive) -or $newest.LastWriteTime -gt (Get-Item $archive).LastWriteTime) {
    Write-Host "building the library" -ForegroundColor Cyan
    & "$PSScriptRoot/mklib.ps1" -NoVerify
    if (-not $?) { exit 1 }
}

New-Item -ItemType Directory -Force build/examples | Out-Null
$cres = "build/examples/$Name.cres"
$defs = @()
foreach ($d in $Define) { $defs += "-D"; $defs += $d }

# Compiled together with the library only to get the declarations file that names its symbols; just the
# example's own .casm is assembled and the rest is taken from the archive.
$argList = @($src) + $AllC + $defs + @("-I", "include", "-O$Level", "-Werror", "-S", "-o", $cres)
$code = Invoke-Tool $Ceresc ($argList -join " ") "build/examples/$Name.compile.out" "build/examples/$Name.compile.err"
$own = [System.IO.Path]::ChangeExtension($src, ".casm")
if ($code -ne 0) {
    Get-Content "build/examples/$Name.compile.err" | Select-Object -First 8 | ForEach-Object { Write-Host $_ -ForegroundColor Red }
    exit 1
}
& $Ceres asm -c $own -o "build/examples/$Name.cobj"
$code = $LASTEXITCODE
foreach ($c in ($AllC + $src)) { Remove-Item ([System.IO.Path]::ChangeExtension($c, ".casm")) -ErrorAction SilentlyContinue }
Get-ChildItem build/examples -Filter "*.decls.casm" | Remove-Item -ErrorAction SilentlyContinue
if ($code -ne 0) { exit $code }
& $Ceres link "build/examples/$Name.cobj" $archive -o $cres
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host ("built $cres ({0} bytes)" -f (Get-Item $cres).Length) -ForegroundColor Green
if ($NoRun) { exit 0 }

$runArgs = @("run", $cres)
if ($Window) { $runArgs += "--window" }
& $Ceres @runArgs
exit $LASTEXITCODE
