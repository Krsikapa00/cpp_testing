<#
Checks this device is ready to work on the repo: git, PlatformIO, host compiler, a clean firmware
build with no warnings, and the PC tests. With -Board it also flashes the board and runs board-check.ps1.

    powershell -ExecutionPolicy Bypass -File scripts/check-setup.ps1
    powershell -ExecutionPolicy Bypass -File scripts/check-setup.ps1 -Board

Run it after switching devices or pulling changes. Exits with 1 if anything failed.
#>
param([switch]$Board)

. "$PSScriptRoot/tools.ps1"
Set-Location $RepoRoot

$results = New-Object System.Collections.Generic.List[object]
function Add-Result([string]$Step, [string]$Status, [string]$Detail) {
    $results.Add([pscustomobject]@{ Step = $Step; Status = $Status; Detail = $Detail })
    Write-Host ("[{0}] {1}: {2}" -f $Status, $Step, $Detail)
}

# Runs a native command, echoing nothing, and returns its output as plain strings.
function Invoke-Quiet([string]$Exe, [string[]]$Arguments) {
    & $Exe @Arguments 2>&1 | ForEach-Object { "$_" }
}

# --- Git ---------------------------------------------------------------------------------------
if (Get-Command git -ErrorAction SilentlyContinue) {
    $null = Invoke-Quiet 'git' @('fetch', '--quiet')
    $fetchOk = ($LASTEXITCODE -eq 0)
    $null = Invoke-Quiet 'git' @('submodule', 'update', '--init', '--recursive')
    $branch = (Invoke-Quiet 'git' @('status', '-sb') | Select-Object -First 1)
    $changes = @(Invoke-Quiet 'git' @('status', '--porcelain')).Count
    if ($branch -match 'behind') {
        Add-Result 'Git' 'WARN' "$branch - run 'git pull' to get the latest work"
    } elseif ($branch -match 'ahead') {
        Add-Result 'Git' 'WARN' "$branch - push before switching devices"
    } elseif (-not $fetchOk) {
        Add-Result 'Git' 'WARN' "$branch - couldn't reach the remote, so this may be out of date"
    } else {
        Add-Result 'Git' 'PASS' "$branch, $changes uncommitted file(s)"
    }
} else {
    Add-Result 'Git' 'FAIL' 'git not found on PATH'
}

# --- PlatformIO --------------------------------------------------------------------------------
$pio = Get-PioCommand
if (-not $pio) {
    Add-Result 'PlatformIO' 'FAIL' 'pio not found. Install the PlatformIO IDE extension in VS Code and let it finish.'
} else {
    $version = (Invoke-Quiet $pio @('--version') | Select-Object -Last 1)
    Add-Result 'PlatformIO' 'PASS' $version
}

# --- Host compiler (for PC tests) --------------------------------------------------------------
$gxx = Get-Command g++ -ErrorAction SilentlyContinue
if ($gxx) {
    Add-Result 'Host compiler' 'PASS' (Invoke-Quiet 'g++' @('--version') | Select-Object -First 1)
} else {
    Add-Result 'Host compiler' 'FAIL' 'g++ not on PATH, so PC tests cannot run. See docs/environment-setup.md.'
}

# --- Firmware build ----------------------------------------------------------------------------
if ($pio) {
    Write-Host 'Building firmware from clean (the first run on a device downloads packages)...'
    $null = Invoke-Quiet $pio @('run', '-e', 'nucleo_f446re', '-t', 'clean')
    $build = Invoke-Quiet $pio @('run', '-e', 'nucleo_f446re')
    $buildExit = $LASTEXITCODE
    $warnings = @($build | Where-Object { $_ -match ': warning:' } | Sort-Object -Unique)
    if ($buildExit -ne 0) {
        $build | Where-Object { $_ -match 'error' } | Select-Object -First 10 | ForEach-Object { Write-Host "    $_" }
        Add-Result 'Firmware build' 'FAIL' 'build failed (errors above)'
    } elseif ($warnings.Count -gt 0) {
        $warnings | ForEach-Object { Write-Host "    $_" }
        Add-Result 'Firmware build' 'WARN' "$($warnings.Count) warning(s) (above)"
    } else {
        $size = ($build | Where-Object { $_ -match '^(RAM|Flash):' }) -join '; '
        Add-Result 'Firmware build' 'PASS' "no warnings. $size"
    }
}

# --- PC tests ----------------------------------------------------------------------------------
if ($pio -and $gxx) {
    Write-Host 'Running PC tests...'
    $tests = Invoke-Quiet $pio @('test', '-e', 'native')
    $summary = ($tests | Where-Object { $_ -match 'test cases' } | Select-Object -Last 1)
    if ($LASTEXITCODE -eq 0) {
        Add-Result 'PC tests' 'PASS' ($summary -replace '=', '').Trim()
    } else {
        $tests | Where-Object { $_ -match 'FAIL|error' } | Select-Object -First 10 | ForEach-Object { Write-Host "    $_" }
        Add-Result 'PC tests' 'FAIL' 'see failures above'
    }
}

# --- Board -------------------------------------------------------------------------------------
if ($Board -and $pio) {
    Write-Host 'Flashing the board...'
    $upload = Invoke-Quiet $pio @('run', '-e', 'nucleo_f446re', '-t', 'upload')
    if ($LASTEXITCODE -eq 0 -and ($upload -match 'Verified OK')) {
        Add-Result 'Flash' 'PASS' 'Verified OK'
        & "$PSScriptRoot/board-check.ps1"
        if ($LASTEXITCODE -eq 0) {
            Add-Result 'Board check' 'PASS' 'registers match (details above)'
        } else {
            Add-Result 'Board check' 'FAIL' 'see board-check output above'
        }
    } else {
        $upload | Where-Object { $_ -match 'rror|not found|No device' } | Select-Object -First 5 | ForEach-Object { Write-Host "    $_" }
        Add-Result 'Flash' 'FAIL' 'upload failed. Is the board plugged in, with the ST-LINK driver installed?'
    }
} elseif (-not $Board) {
    Write-Host 'Skipped the board. Add -Board to flash it and check its registers.'
}

# --- Summary -----------------------------------------------------------------------------------
Write-Host ''
$results | Format-Table -AutoSize | Out-String | Write-Host
if ($results | Where-Object { $_.Status -eq 'FAIL' }) { exit 1 }
exit 0
