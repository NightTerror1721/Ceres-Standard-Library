<#
.SYNOPSIS
    Checks the text interface against a real Windows console: keys reach a program as they are pressed.

.DESCRIPTION
    A pipe cannot show this, and neither can the test runner (its tests read a file). A console hands a program
    whole lines, once Enter is pressed, and keeps the arrow keys for its own line editor - which is how a menu
    once moved on every letter of a line and then chose. This builds tools/console/tuidemo.c (two menus, then an
    ordinary line read), runs it in a hidden console, types into that console with real key events
    (tools/console/conharness.cpp), and requires:

      - Down, Down, Enter chooses the third item of the first menu;
      - End, Up, Enter chooses the third of the second (End and Up are keys the console's line editor keeps);
      - a line typed afterwards is read as a line (raw keys are given back);
      - the console's input mode is what it was before the program ran.

    Needs g++ on the PATH to build the harness; without it this says so and exits 0.

.EXAMPLE
    tools\consolecheck.ps1
#>
[CmdletBinding()]
param([int]$TimeoutSeconds = 300)

. "$PSScriptRoot/common.ps1"

$gxx = Get-Command g++ -ErrorAction SilentlyContinue
if (-not $gxx) { Write-Host "SKIP: g++ is needed to build the console harness"; exit 0 }

$dir = 'build/console'
New-Item -ItemType Directory -Force $dir | Out-Null
$harness = "$dir/conharness.exe"
if (-not (Test-Path $harness) -or (Get-Item $harness).LastWriteTime -lt (Get-Item 'tools/console/conharness.cpp').LastWriteTime) {
    & g++ -O1 -o $harness tools/console/conharness.cpp
    if ($LASTEXITCODE -ne 0) { Write-Host "FAILED: could not build the console harness" -ForegroundColor Red; exit 1 }
}

$sources = @($CoreC + $Asm)
$args1 = @(($TimeoutSeconds * 1000).ToString(),
    'waitfor:Colour', 'key:DOWN', 'key:DOWN', 'wait:300', 'key:ENTER',
    'waitfor:Again', 'wait:300', 'key:END', 'key:UP', 'wait:200', 'key:ENTER',
    'waitfor:type a line', 'text:hello', 'key:ENTER', 'wait:800',
    '--', $Ceresc) + $sources + @('tools/console/tuidemo.c', '-I', 'include', '-O1', '-o', "$dir/tuidemo.cres",
    '--run', '--clean', '--ceres-path', $CeresDir)

$output = & $harness @args1 2>&1 | Out-String
$failures = @()
if ($output -notmatch 'chosen 2 2')        { $failures += "the two menus did not choose 2 and 2 (keys did not arrive as they were pressed)" }
if ($output -notmatch 'line \[hello')      { $failures += "the line read after the menus did not get 'hello' (raw keys were not given back)" }
if ($output -notmatch 'console input mode after exit: 000001f7') { $failures += "the console's input mode was not restored" }
if ($output -notmatch 'RESULT: exited with 0') { $failures += "the program did not exit with 0" }

if ($failures.Count -eq 0) {
    Write-Host "ok    tui in a real console: arrows, Home/End and Enter as pressed; a line afterwards; console restored" -ForegroundColor Green
    exit 0
}
$failures | ForEach-Object { Write-Host "FAIL  $_" -ForegroundColor Red }
Write-Host $output
exit 1
