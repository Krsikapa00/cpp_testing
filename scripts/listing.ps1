<#
Builds the firmware and writes a disassembly listing: the vector table first, then all code mixed
with source lines. The file goes in .pio/build/nucleo_f446re/firmware.lst, which git ignores.

    powershell -ExecutionPolicy Bypass -File scripts/listing.ps1
    powershell -ExecutionPolicy Bypass -File scripts/listing.ps1 -NoBuild   # use the existing build
#>
param([switch]$NoBuild)

. "$PSScriptRoot/tools.ps1"
Set-Location $RepoRoot

if (-not $NoBuild) {
    $pio = Get-PioCommand
    if (-not $pio) { Write-Host 'pio not found. Install the PlatformIO IDE extension in VS Code.'; exit 1 }
    & $pio run -e nucleo_f446re
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$objdump = Get-PackageTool 'toolchain-gccarmnoneeabi' 'arm-none-eabi-objdump'
if (-not $objdump -or -not (Test-Path $ElfPath)) {
    Write-Host 'No firmware or objdump yet. Build once with "pio run" first.'
    exit 1
}

$listing = Join-Path $RepoRoot '.pio/build/nucleo_f446re/firmware.lst'
& $objdump -s -j .isr_vector $ElfPath | Out-File -Encoding utf8 $listing
& $objdump -d -S -C $ElfPath | Out-File -Encoding utf8 -Append $listing
Write-Host "Listing written to $listing"

if (Get-Command code -ErrorAction SilentlyContinue) { code $listing }
