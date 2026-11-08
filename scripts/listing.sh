#!/usr/bin/env bash
# Builds the firmware and writes a disassembly listing: the vector table first, then all code mixed
# with source lines. The file goes in .pio/build/nucleo_f446re/firmware.lst, which git ignores.
#
#     bash scripts/listing.sh
#     bash scripts/listing.sh --no-build   # use the existing build
#
# Linux version of listing.ps1.

. "$(dirname "$0")/tools.sh"
cd "$REPO_ROOT" || exit 1

if [ "${1:-}" != "--no-build" ]; then
    PIO="$(pio_command)"
    if [ -z "$PIO" ]; then echo "pio not found. Install the PlatformIO IDE extension in VS Code."; exit 1; fi
    "$PIO" run -e nucleo_f446re || exit $?
fi

OBJDUMP="$(package_tool toolchain-gccarmnoneeabi arm-none-eabi-objdump)"
if [ -z "$OBJDUMP" ] || [ ! -f "$ELF_PATH" ]; then
    echo 'No firmware or objdump yet. Build once with "pio run" first.'
    exit 1
fi

LISTING="$REPO_ROOT/.pio/build/nucleo_f446re/firmware.lst"
{
    "$OBJDUMP" -s -j .isr_vector "$ELF_PATH"
    "$OBJDUMP" -d -S -C "$ELF_PATH"
} > "$LISTING"
echo "Listing written to $LISTING"

if command -v code >/dev/null 2>&1; then code "$LISTING"; fi
