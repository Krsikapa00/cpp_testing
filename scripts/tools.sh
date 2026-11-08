# Shared paths for the bash scripts in this folder. Source it: . "$(dirname "$0")/tools.sh"
# Linux version of tools.ps1. Also works in Git Bash on Windows.

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PIO_CORE="${PLATFORMIO_CORE_DIR:-$HOME/.platformio}"
ELF_PATH="$REPO_ROOT/.pio/build/nucleo_f446re/firmware.elf"

# Prints the path if it's executable, allowing for the .exe suffix under Git Bash.
find_exe() {
    for candidate in "$1" "$1.exe"; do
        if [ -x "$candidate" ]; then echo "$candidate"; return 0; fi
    done
    return 1
}

# The pio command, even when it isn't on PATH (the VS Code extension doesn't add it).
pio_command() {
    command -v pio 2>/dev/null && return 0
    find_exe "$PIO_CORE/penv/bin/pio" || find_exe "$PIO_CORE/penv/Scripts/pio"
}

# A tool from a PlatformIO package, such as OpenOCD or the Arm binutils. Fails until PlatformIO installs it.
package_tool() {
    find_exe "$PIO_CORE/packages/$1/bin/$2"
}
