<#
.SYNOPSIS
    Installs the library as a sysroot: the headers, the archive and its declarations, in the layout ceresc's
    --sysroot and -l expect.

.DESCRIPTION
    Builds what is missing (tools/mklib.ps1, -O2, and the -fsoft-double variant) and writes:

        <Prefix>/include/                     every header
        <Prefix>/lib/libceres.car             the library, -O2
        <Prefix>/lib/libceres.decls.casm      its declarations (ceresc takes them along with -lceres)
        <Prefix>/lib/soft-double/...          the same, built with -fsoft-double (ceresc looks there first when a
                                              program is compiled with -fsoft-double)
        <Prefix>/lib/libceres_<module>.cobj   the optional modules, which bind interrupt vectors and so are linked
                                              only when named: -lceres_irq; -lceres_fault -lceres_fault_asm;
                                              -lceres_mmu -lceres_mmu_asm

    A program is then built with

        ceresc prog.c --sysroot <Prefix> -lceres -O2 --run

    and nothing else to say where the library is.

.EXAMPLE
    tools\install.ps1 -Prefix C:\ceres\sysroot
    tools\install.ps1 -Prefix build/sysroot -NoSoftDouble
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Prefix,
    [switch]$NoSoftDouble
)

. "$PSScriptRoot/common.ps1"

Ensure-Library @(2)
$lib = Get-LibraryDir 2
if (-not $NoSoftDouble) {
    & "$PSScriptRoot/mklib.ps1" -Levels 2 -SoftDouble -NoVerify
    if (-not $?) { throw "tools/mklib.ps1 -SoftDouble failed" }
}

$includeOut = Join-Path $Prefix 'include'
$libOut = Join-Path $Prefix 'lib'
if (Test-Path $includeOut) { Remove-Item $includeOut -Recurse -Force }
New-Item -ItemType Directory -Force $includeOut, $libOut | Out-Null
Copy-Item "$Root/include/*" $includeOut -Recurse -Force

Copy-Item "$lib/libceres.car" (Join-Path $libOut 'libceres.car') -Force
Copy-Item "$lib/libceres.decls.casm" (Join-Path $libOut 'libceres.decls.casm') -Force

foreach ($module in $Optional.Keys) {
    foreach ($file in $Optional[$module]) {
        $suffix = if ($file -like '*.casm') { "_asm" } else { "" }
        Copy-Item "$lib/obj/$(Get-FlatName $file).cobj" (Join-Path $libOut "libceres_$module$suffix.cobj") -Force
    }
}

if (-not $NoSoftDouble) {
    $sd = Join-Path $libOut 'soft-double'
    New-Item -ItemType Directory -Force $sd | Out-Null
    Copy-Item "$(Get-LibraryDir 2)-sd/libceres.car" (Join-Path $sd 'libceres.car') -Force
    Copy-Item "$(Get-LibraryDir 2)-sd/libceres.decls.casm" (Join-Path $sd 'libceres.decls.casm') -Force
}

Write-Host "installed the library in $Prefix" -ForegroundColor Green
Write-Host "  ceresc prog.c --sysroot $Prefix -lceres -O2 --run" -ForegroundColor DarkGray
