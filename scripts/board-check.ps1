<#
Reads registers from the running board through its ST-LINK and compares them with what the current
project should produce. Flash first (pio run -t upload), then:

    powershell -ExecutionPolicy Bypass -File scripts/board-check.ps1

The expected values are for Project 1: 180 MHz clock and LD2 blinking. Update $checks when a
project changes what the board should be doing. Exits with 1 if any check fails.
#>

. "$PSScriptRoot/tools.ps1"

$openocd = Get-PackageTool 'tool-openocd' 'openocd'
$nm = Get-PackageTool 'toolchain-gccarmnoneeabi' 'arm-none-eabi-nm'
if (-not $openocd) {
    Write-Host '[FAIL] OpenOCD not found. Run "pio run -t upload" once so PlatformIO installs it.'
    exit 1
}
$openocdScripts = Join-Path (Split-Path (Split-Path $openocd -Parent) -Parent) 'openocd/scripts'

# Values are hex strings, because PowerShell reads literals like 0xA8000400 as negative numbers.
$checks = New-Object System.Collections.Generic.List[object]
function Add-Check([string]$Name, [string]$Address, [string]$Mask, [string]$Expected, [string]$Meaning) {
    $checks.Add([pscustomobject]@{ Name = $Name; Address = $Address; Mask = $Mask; Expected = $Expected; Meaning = $Meaning })
}
Add-Check 'RCC_PLLCFGR' '40023804' 'FFFFFFFF' '27405A08' 'PLL from HSE: M=8, N=360, P=2, Q=7, R=2'
Add-Check 'RCC_CFGR'    '40023808' 'FFFFFFFF' '0000940A' 'Running on the PLL, AHB /1, APB1 /4, APB2 /2'
Add-Check 'FLASH_ACR'   '40023C00' 'FFFFFFFF' '00000705' '5 wait states, prefetch and caches on'
Add-Check 'CPACR'       'E000ED88' '00F00000' '00F00000' 'FPU enabled'
Add-Check 'CFSR'        'E000ED28' 'FFFFFFFF' '00000000' 'No configurable faults'
Add-Check 'HFSR'        'E000ED2C' 'FFFFFFFF' '00000000' 'No hard fault'
Add-Check 'GPIOA_MODER' '40020000' 'FC000C00' 'A8000400' 'Debug pins PA13-15 kept, PA5 an output'

# SystemCoreClock's address comes from the ELF, so it must be the build that's on the board.
if ($nm -and (Test-Path $ElfPath)) {
    $line = & $nm $ElfPath 2>$null | Where-Object { $_ -match ' SystemCoreClock$' } | Select-Object -First 1
    if ($line) {
        Add-Check 'SystemCoreClock' $line.Split(' ')[0] 'FFFFFFFF' '0ABA9500' '180000000 (from the current build''s ELF)'
    }
}

# The LED is sampled for 5 s, longer than the slowest blink so far (the busy-wait loop toggles about every 3.5 s).
$odrSamples = 10
$sampleMs = 500
$ocdArgs = @('-s', $openocdScripts, '-f', 'board/st_nucleo_f4.cfg', '-c', 'init', '-c', 'halt')
foreach ($check in $checks) { $ocdArgs += @('-c', "mdw 0x$($check.Address)") }
for ($i = 0; $i -lt $odrSamples; $i++) {
    $ocdArgs += @('-c', 'resume', '-c', "sleep $sampleMs", '-c', 'halt', '-c', 'mdw 0x40020014')
}
$ocdArgs += @('-c', 'resume', '-c', 'shutdown')

$output = & $openocd @ocdArgs 2>&1 | ForEach-Object { "$_" }
$values = @($output | Where-Object { $_ -match '^0x[0-9a-fA-F]+:\s+([0-9a-fA-F]+)' } | ForEach-Object {
    [Convert]::ToUInt32(($_ -split '\s+')[1], 16)
})

if ($values.Count -ne ($checks.Count + $odrSamples)) {
    Write-Host '[FAIL] Could not read the board. Is it plugged in and not held by a debug session? OpenOCD said:'
    $output | Where-Object { $_ -match 'rror|unable|failed' } | Select-Object -First 8 | ForEach-Object { Write-Host "    $_" }
    exit 1
}

$failed = $false
$rows = for ($i = 0; $i -lt $checks.Count; $i++) {
    $check = $checks[$i]
    $mask = [Convert]::ToUInt32($check.Mask, 16)
    $read = [uint32]$values[$i]
    $ok = (([int64]$read -band [int64]$mask) -eq [int64][Convert]::ToUInt32($check.Expected, 16))
    if (-not $ok) { $failed = $true }
    [pscustomobject]@{
        Result   = if ($ok) { 'PASS' } else { 'FAIL' }
        Register = $check.Name
        Read     = '0x{0:X8}' -f $read
        Expected = if ($check.Mask -eq 'FFFFFFFF') { "0x$($check.Expected)" } else { "0x$($check.Expected) (mask 0x$($check.Mask))" }
        Meaning  = $check.Meaning
    }
}
$rows | Format-Table -AutoSize | Out-String | Write-Host

# LD2 is PA5, bit 5 of GPIOA_ODR. It should change at least once across the samples.
$ledStates = @($values | Select-Object -Last $odrSamples | ForEach-Object { ([int64]$_ -band 0x20) -ne 0 } | Sort-Object -Unique)
if ($ledStates.Count -gt 1) {
    Write-Host "[PASS] LD2 changed state during $($odrSamples * $sampleMs) ms of sampling, so the main loop is running."
} else {
    Write-Host "[WARN] LD2 stayed $(if ($ledStates[0]) { 'on' } else { 'off' }) for $($odrSamples * $sampleMs) ms. Fine only if your code isn't blinking it."
}

if ($failed) { exit 1 }
exit 0
