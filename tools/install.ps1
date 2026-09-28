<#
.SYNOPSIS
    Installs the library as a sysroot: the headers, the archive and its declarations, in the layout ceresc's
    --sysroot and -l expect.

.DESCRIPTION
    Builds what is missing (tools/mklib.ps1, -O2) and writes:

        <Prefix>/include/                     every header
        <Prefix>/lib/libceres.car             the library, -O2
        <Prefix>/lib/libceres.decls.casm      its declarations (ceresc takes them along with -lceres)
        <Prefix>/lib/libceres_<module>.cobj   the optional modules, which bind interrupt vectors and so are linked
                                              only when named: -lceres_irq; -lceres_fault -lceres_fault_asm;
                                              -lceres_mmu -lceres_mmu_asm

    A program is then built with

        ceresc prog.c --sysroot <Prefix> -lceres -O2 --run

    and nothing else to say where the library is.

.EXAMPLE
    tools\install.ps1 -Prefix C:\ceres\sysroot
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Prefix
)

. "$PSScriptRoot/common.ps1"

Ensure-Library @(2)
$lib = Get-LibraryDir 2

# The sysroot's include/ is replaced whole, so it must not be this checkout's own: a Prefix that is the checkout or
# holds it would delete the headers being installed.
$rootFull = [System.IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
$prefixFull = [System.IO.Path]::GetFullPath((Join-Path (Get-Location).Path $Prefix)).TrimEnd('\', '/')
$separator = [System.IO.Path]::DirectorySeparatorChar
if ($rootFull.Equals($prefixFull, [System.StringComparison]::OrdinalIgnoreCase) -or
    $rootFull.StartsWith($prefixFull + $separator, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "-Prefix $Prefix is this checkout or holds it: installing would delete its own include/"
}

$includeOut = Join-Path $Prefix 'include'
$libOut = Join-Path $Prefix 'lib'
if (Test-Path $includeOut) { Remove-Item $includeOut -Recurse -Force }
New-Item -ItemType Directory -Force $includeOut, $libOut | Out-Null
# What an earlier install put in lib/: an optional module it had and this one has not would still link.
Get-ChildItem $libOut -Filter 'libceres*' -File | Remove-Item -Force
# (lib/soft-double/ is what installs made while there was a -fsoft-double library.)
if (Test-Path (Join-Path $libOut 'soft-double')) { Remove-Item (Join-Path $libOut 'soft-double') -Recurse -Force }
Copy-Item "$Root/include/*" $includeOut -Recurse -Force

Copy-Item "$lib/libceres.car" (Join-Path $libOut 'libceres.car') -Force
Copy-Item "$lib/libceres.decls.casm" (Join-Path $libOut 'libceres.decls.casm') -Force

foreach ($module in $Optional.Keys) {
    foreach ($file in $Optional[$module]) {
        $suffix = if ($file -like '*.casm') { "_asm" } else { "" }
        Copy-Item "$lib/obj/$(Get-FlatName $file).cobj" (Join-Path $libOut "libceres_$module$suffix.cobj") -Force
    }
}

Write-Host "installed the library in $Prefix" -ForegroundColor Green
Write-Host "  ceresc prog.c --sysroot $Prefix -lceres -O2 --run" -ForegroundColor DarkGray
