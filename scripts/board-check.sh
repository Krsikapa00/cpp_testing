#!/usr/bin/env bash
# Reads registers from the running board through its ST-LINK and compares them with what the current
# project should produce. Flash first (pio run -t upload), then:
#
#     bash scripts/board-check.sh
#
# Linux version of board-check.ps1. The expected values are for Project 1: 180 MHz clock and LD2
# blinking. Update CHECKS here and in board-check.ps1 when a project changes what the board should do.
# Exits with 1 if any check fails.

. "$(dirname "$0")/tools.sh"

OPENOCD="$(package_tool tool-openocd openocd)"
NM="$(package_tool toolchain-gccarmnoneeabi arm-none-eabi-nm)"
if [ -z "$OPENOCD" ]; then
    echo '[FAIL] OpenOCD not found. Run "pio run -t upload" once so PlatformIO installs it.'
    exit 1
fi
OPENOCD_SCRIPTS="$(dirname "$(dirname "$OPENOCD")")/openocd/scripts"

# name|address|mask|expected|meaning, all values in hex
CHECKS=(
    "RCC_PLLCFGR|40023804|FFFFFFFF|27405A08|PLL from HSE: M=8, N=360, P=2, Q=7, R=2"
    "RCC_CFGR|40023808|FFFFFFFF|0000940A|Running on the PLL, AHB /1, APB1 /4, APB2 /2"
    "FLASH_ACR|40023C00|FFFFFFFF|00000705|5 wait states, prefetch and caches on"
    "CPACR|E000ED88|00F00000|00F00000|FPU enabled"
    "CFSR|E000ED28|FFFFFFFF|00000000|No configurable faults"
    "HFSR|E000ED2C|FFFFFFFF|00000000|No hard fault"
    "GPIOA_MODER|40020000|FC000C00|A8000400|Debug pins PA13-15 kept, PA5 an output"
)

# SystemCoreClock's address comes from the ELF, so it must be the build that's on the board.
if [ -n "$NM" ] && [ -f "$ELF_PATH" ]; then
    clock_addr="$("$NM" "$ELF_PATH" 2>/dev/null | tr -d '\r' | awk '$3 == "SystemCoreClock" { print $1; exit }')"
    if [ -n "$clock_addr" ]; then
        CHECKS+=("SystemCoreClock|$clock_addr|FFFFFFFF|0ABA9500|180000000 (from the current build's ELF)")
    fi
fi

# The LED is sampled for 5 s, longer than the slowest blink so far (the busy-wait loop toggles about every 3.5 s).
ODR_SAMPLES=10
SAMPLE_MS=500
args=(-s "$OPENOCD_SCRIPTS" -f board/st_nucleo_f4.cfg -c init -c halt)
for check in "${CHECKS[@]}"; do
    IFS='|' read -r _ addr _ _ _ <<<"$check"
    args+=(-c "mdw 0x$addr")
done
for ((i = 0; i < ODR_SAMPLES; i++)); do
    args+=(-c resume -c "sleep $SAMPLE_MS" -c halt -c "mdw 0x40020014")
done
args+=(-c resume -c shutdown)

output="$("$OPENOCD" "${args[@]}" 2>&1 | tr -d '\r')"
mapfile -t values < <(grep -E '^0x[0-9a-fA-F]+:' <<<"$output" | awk '{ print $2 }')

if [ "${#values[@]}" -ne $(( ${#CHECKS[@]} + ODR_SAMPLES )) ]; then
    echo '[FAIL] Could not read the board. Is it plugged in and not held by a debug session? OpenOCD said:'
    grep -iE 'rror|unable|failed' <<<"$output" | head -n 8 | sed 's/^/    /'
    exit 1
fi

FAILED=0
printf '%-6s %-16s %-12s %-30s %s\n' "Result" "Register" "Read" "Expected" "Meaning"
for i in "${!CHECKS[@]}"; do
    IFS='|' read -r name _ mask expected meaning <<<"${CHECKS[$i]}"
    read_value="${values[$i]}"
    if (( (0x$read_value & 0x$mask) == 0x$expected )); then
        status="PASS"
    else
        status="FAIL"
        FAILED=1
    fi
    if [ "$mask" = "FFFFFFFF" ]; then shown="0x$expected"; else shown="0x$expected (mask 0x$mask)"; fi
    printf '%-6s %-16s 0x%-10s %-30s %s\n' "$status" "$name" "$(tr 'a-f' 'A-F' <<<"$read_value")" "$shown" "$meaning"
done
echo

# LD2 is PA5, bit 5 of GPIOA_ODR. It should change at least once across the samples.
led_states="$(for v in "${values[@]: -$ODR_SAMPLES}"; do echo $(( (0x$v >> 5) & 1 )); done | sort -u | wc -l | tr -d ' ')"
if [ "$led_states" -gt 1 ]; then
    echo "[PASS] LD2 changed state during $(( ODR_SAMPLES * SAMPLE_MS )) ms of sampling, so the main loop is running."
else
    echo "[WARN] LD2 didn't change for $(( ODR_SAMPLES * SAMPLE_MS )) ms. Fine only if your code isn't blinking it."
fi

exit "$FAILED"
