#!/usr/bin/env bash
# Checks this device is ready to work on the repo: git, PlatformIO, host compiler, a clean firmware
# build with no warnings, and the PC tests. With --board it also flashes the board and runs board-check.sh.
#
#     bash scripts/check-setup.sh
#     bash scripts/check-setup.sh --board
#
# Linux version of check-setup.ps1. Run it after switching devices or pulling changes.
# Exits with 1 if anything failed.

BOARD=0
[ "${1:-}" = "--board" ] && BOARD=1

. "$(dirname "$0")/tools.sh"
cd "$REPO_ROOT" || exit 1

SUMMARY=()
FAILED=0
result() {
    printf '[%s] %s: %s\n' "$2" "$1" "$3"
    SUMMARY+=("$(printf '%-16s %-5s %s' "$1" "$2" "$3")")
    if [ "$2" = "FAIL" ]; then FAILED=1; fi
}

# --- Git -----------------------------------------------------------------------------------------
if command -v git >/dev/null 2>&1; then
    git fetch --quiet >/dev/null 2>&1
    fetch_ok=$?
    git submodule update --init --recursive >/dev/null 2>&1
    branch="$(git status -sb | head -n 1)"
    changes="$(git status --porcelain | wc -l | tr -d ' ')"
    if [[ "$branch" == *behind* ]]; then
        result "Git" "WARN" "$branch - run 'git pull' to get the latest work"
    elif [[ "$branch" == *ahead* ]]; then
        result "Git" "WARN" "$branch - push before switching devices"
    elif [ "$fetch_ok" -ne 0 ]; then
        result "Git" "WARN" "$branch - couldn't reach the remote, so this may be out of date"
    else
        result "Git" "PASS" "$branch, $changes uncommitted file(s)"
    fi
else
    result "Git" "FAIL" "git not found on PATH"
fi

# --- PlatformIO ----------------------------------------------------------------------------------
PIO="$(pio_command)"
if [ -z "$PIO" ]; then
    result "PlatformIO" "FAIL" "pio not found. Install the PlatformIO IDE extension in VS Code and let it finish."
else
    result "PlatformIO" "PASS" "$("$PIO" --version 2>&1 | tail -n 1)"
fi

# --- Host compiler (for PC tests) ----------------------------------------------------------------
HAVE_GXX=0
if command -v g++ >/dev/null 2>&1; then
    HAVE_GXX=1
    result "Host compiler" "PASS" "$(g++ --version | head -n 1)"
else
    result "Host compiler" "FAIL" "g++ not on PATH, so PC tests cannot run. Install build-essential."
fi

# --- Firmware build ------------------------------------------------------------------------------
if [ -n "$PIO" ]; then
    echo "Building firmware from clean (the first run on a device downloads packages)..."
    "$PIO" run -e nucleo_f446re -t clean >/dev/null 2>&1
    build="$("$PIO" run -e nucleo_f446re 2>&1)"
    build_exit=$?
    warnings="$(grep ': warning:' <<<"$build" | sort -u)"
    if [ "$build_exit" -ne 0 ]; then
        grep -i 'error' <<<"$build" | head -n 10 | sed 's/^/    /'
        result "Firmware build" "FAIL" "build failed (errors above)"
    elif [ -n "$warnings" ]; then
        sed 's/^/    /' <<<"$warnings"
        result "Firmware build" "WARN" "$(wc -l <<<"$warnings" | tr -d ' ') warning(s) (above)"
    else
        size="$(grep -E '^(RAM|Flash):' <<<"$build" | paste -sd ';' -)"
        result "Firmware build" "PASS" "no warnings. $size"
    fi
fi

# --- PC tests ------------------------------------------------------------------------------------
if [ -n "$PIO" ] && [ "$HAVE_GXX" -eq 1 ]; then
    echo "Running PC tests..."
    tests="$("$PIO" test -e native 2>&1)"
    if [ $? -eq 0 ]; then
        result "PC tests" "PASS" "$(grep 'test cases' <<<"$tests" | tail -n 1 | tr -d '=' | sed 's/^ *//;s/ *$//')"
    else
        grep -E 'FAIL|error' <<<"$tests" | head -n 10 | sed 's/^/    /'
        result "PC tests" "FAIL" "see failures above"
    fi
fi

# --- Board ---------------------------------------------------------------------------------------
if [ "$BOARD" -eq 1 ] && [ -n "$PIO" ]; then
    echo "Flashing the board..."
    upload="$("$PIO" run -e nucleo_f446re -t upload 2>&1)"
    if [ $? -eq 0 ] && grep -q 'Verified OK' <<<"$upload"; then
        result "Flash" "PASS" "Verified OK"
        if bash "$(dirname "$0")/board-check.sh"; then
            result "Board check" "PASS" "registers match (details above)"
        else
            result "Board check" "FAIL" "see board-check output above"
        fi
    else
        grep -iE 'rror|not found|no device|permission' <<<"$upload" | head -n 5 | sed 's/^/    /'
        result "Flash" "FAIL" "upload failed. Is the board plugged in? On Linux, check the PlatformIO udev rules are installed."
    fi
elif [ "$BOARD" -eq 0 ]; then
    echo "Skipped the board. Add --board to flash it and check its registers."
fi

# --- Summary -------------------------------------------------------------------------------------
echo
printf '%s\n' "${SUMMARY[@]}"
exit "$FAILED"
