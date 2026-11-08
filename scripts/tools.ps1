# Shared paths for the scripts in this folder. Dot-source it: . "$PSScriptRoot/tools.ps1"
# Works in Windows PowerShell 5.1 and PowerShell 7 on any OS.

$RepoRoot = Split-Path $PSScriptRoot -Parent
$ExeSuffix = if ($env:OS -eq 'Windows_NT') { '.exe' } else { '' }
$PioCore = if ($env:PLATFORMIO_CORE_DIR) { $env:PLATFORMIO_CORE_DIR } else { Join-Path $HOME '.platformio' }
$ElfPath = Join-Path $RepoRoot '.pio/build/nucleo_f446re/firmware.elf'

# The pio command, even when it isn't on PATH (the VS Code extension doesn't add it).
function Get-PioCommand {
    $cmd = Get-Command pio -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    foreach ($dir in 'Scripts', 'bin') {
        $candidate = Join-Path $PioCore "penv/$dir/pio$ExeSuffix"
        if (Test-Path $candidate) { return $candidate }
    }
    return $null
}

# A tool from a PlatformIO package, such as OpenOCD or the Arm binutils. Returns $null until PlatformIO installs it.
function Get-PackageTool([string]$Package, [string]$Tool) {
    $path = Join-Path $PioCore "packages/$Package/bin/$Tool$ExeSuffix"
    if (Test-Path $path) { return $path }
    return $null
}
