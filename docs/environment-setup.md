# Environment Setup

How this repo's embedded C++ environment is built, why each piece is there, and how to recreate it on another device or reuse it for a new project.

## What's in the environment

| Piece | Version | Comes from | What it does |
|---|---|---|---|
| VS Code with PlatformIO IDE | Extension 3.3.4 | You install it | Editor, build buttons, debugger UI |
| PlatformIO Core (`pio`) | 6.2.0 | PlatformIO IDE | Build system and package manager |
| STM32 platform | `ststm32` 20.0.0 | `platformio.ini` | Board definitions and build scripts |
| Arm GCC | 7.2.1 (`1.70201.0`) | `platformio.ini` | Cross-compiler for the chip |
| CMSIS | `framework-cmsis-stm32f4` 2.6.11 | STM32 platform | Register headers, startup code, linker script |
| OpenOCD | `tool-openocd` 3.1200.7 | STM32 platform | Flashes and debugs through the ST-LINK |
| Host GCC | 5.4 here, any C++11 compiler works | You install it | Builds unit tests that run on the PC |
| Unity | 2.6.1 | `pio test` | Unit test framework |

PlatformIO downloads everything marked `platformio.ini`, STM32 platform or `pio test` on the first build. Packages live in `~/.platformio`, shared between projects, and build output lives in `.pio/`, which git ignores.

## Set up a new device

1. **Install VS Code and the PlatformIO IDE extension.** Wait for the extension to finish installing PlatformIO Core, then restart VS Code.
2. **Install a host compiler** for the PC tests. It needs to be on `PATH`.
   - **Windows:** install MinGW-w64, for example through [MSYS2](https://www.msys2.org/).
   - **Linux:** `sudo apt install build-essential`
   - **macOS:** `xcode-select --install`
3. **Set up USB access to the ST-LINK.**
   - **Windows:** plug in the board and check Device Manager for "ST-Link Debug" and "STLink Virtual COM Port". If they're missing, install ST's STSW-LINK009 driver.
   - **Linux:** install the [PlatformIO udev rules](https://docs.platformio.org/en/latest/core/installation/udev-rules.html).
4. **Clone the repo.** Use `--recursive`, as the FreeRTOS kernel will be a submodule from Project 2 on.

   ```
   git clone --recursive https://github.com/Krsikapa00/cpp_testing.git
   ```

5. **Open the folder in VS Code** and let PlatformIO finish its first-time setup.
6. **Run the three checks below.** Use the PlatformIO Core CLI terminal from the PlatformIO sidebar if `pio` isn't on your `PATH`.

   | Command | What it proves | Expected result |
   |---|---|---|
   | `pio run` | Toolchain and flags work (the first run downloads packages) | `SUCCESS`, no warnings |
   | `pio test -e native` | PC tests work and build as C++11 | All tests pass |
   | `pio run -t upload` | Flashing works | `** Verified OK **` |

If a download fails with a mirror or connection error, PlatformIO usually retries another mirror by itself. Run the command again if it stops.

## Commands

| Task | Command |
|---|---|
| Build firmware | `pio run` |
| Build and flash | `pio run -t upload` |
| Clean build output | `pio run -t clean` |
| Show exact compiler flags | `pio run -v` |
| Run PC tests | `pio test -e native` |
| Serial monitor at 115200 baud | `pio device monitor` |
| Debug in VS Code | Run and Debug panel, then **PIO Debug** |
| Debug from the terminal | `pio debug --interface=gdb -- -x .pioinit` |

The Arm tools aren't on `PATH`. They're in `~/.platformio/packages/toolchain-gccarmnoneeabi/bin/`:

```powershell
$tc = "$env:USERPROFILE\.platformio\packages\toolchain-gccarmnoneeabi\bin"
$elf = ".pio\build\nucleo_f446re\firmware.elf"

& "$tc\arm-none-eabi-size.exe" $elf            # Flash and RAM use
& "$tc\arm-none-eabi-objdump.exe" -d -S $elf   # Disassembly mixed with source
& "$tc\arm-none-eabi-nm.exe" -n $elf           # Symbols by address
```

The linker's map file is at `.pio/build/nucleo_f446re/firmware.map`.

## How the configuration works

### `platformio.ini`

This defines two environments. `pio run` builds only `nucleo_f446re`, because that's the default.

| Setting | Why |
|---|---|
| `platform = ststm32@20.0.0` | Pinned so every device gets the same platform |
| `platform_packages = ...toolchain-gccarmnoneeabi@1.70201.0` | The platform allows a range of compilers, so this pins the exact one |
| `framework = cmsis` | Register headers and startup code only, with no driver layer |
| `upload_protocol`, `debug_tool = stlink` | Uses the ST-LINK built into the Nucleo |
| `extra_scripts = pre:scripts/build_flags.py` | Adds flags `build_flags` can't route correctly (see below) |
| `build_unflags = -std=gnu++14 -std=gnu++17` | Removes any newer standard PlatformIO might add |
| `-std=gnu++11` | The company's C++ standard |
| `-Wall -Wextra -Wpedantic -Wdouble-promotion` | Stricter warnings, including accidental use of slow `double` |
| `-DHSE_VALUE=8000000U` | Tells CMSIS the true input clock (see [Problems and fixes](#problems-and-fixes)) |
| `[env:native]` | Builds tests for the PC instead of the chip |

PlatformIO's build script already adds `-fno-exceptions`, `-fno-rtti`, `-Os` and `-ffunction-sections -fdata-sections` with `--gc-sections`, which remove unused code. So they aren't repeated here.

### `scripts/build_flags.py`

`build_flags` sends flags to C and C++ compile steps, but not to the linker or assembler. This script adds flags straight to the right tool's list:

| Flag | Goes to | Why |
|---|---|---|
| `-mfpu=fpv4-sp-d16 -mfloat-abi=hard` | Compiler, assembler, linker | Uses the F446's FPU. Every step must agree. |
| `-fno-threadsafe-statics` | C++ only | Removes the guard code around function-local statics, which needs runtime support we don't have |
| `-g` | Compiler | Debug symbols for GDB and `objdump -S`. These stay on the PC and don't use flash. |
| `-Wl,-Map,...` | Linker | Writes the map file |

### Other files

- **`.gitignore`:** ignores `.pio/` and the VS Code files PlatformIO generates for each device.
- **`test/test_toolchain/`:** confirms PC tests run and that the compiler really is in C++11 mode.
- **`src/main.cpp`:** currently a toolchain check that blinks LD2 using a float loop, so the LED only blinks if the FPU works. Project 1 replaces it.

## Reuse it for a new project

1. Copy `platformio.ini`, `scripts/build_flags.py` and `.gitignore`.
2. Update these for your hardware:

   | What | Where | How to find it |
   |---|---|---|
   | Board | `board` and the environment name | `pio boards stm32`, or the [board list](https://docs.platformio.org/en/latest/boards/index.html) |
   | Input clock | `-DHSE_VALUE` | The board's user manual, and check how HSE is wired |
   | FPU | `FPU_FLAGS` in `build_flags.py` | See the table below |
   | C++ standard | `-std=` | Your project's rules |

3. Check for newer pinned versions if you aren't tied to matching this setup.
4. Run `pio run -v` and read the flags on one compile line and the link line.

| Core | FPU flags |
|---|---|
| Cortex-M4F (F4, G4, L4) | `-mfpu=fpv4-sp-d16 -mfloat-abi=hard` |
| Cortex-M7, single precision (some F7) | `-mfpu=fpv5-sp-d16 -mfloat-abi=hard` |
| Cortex-M7, double precision (H7, some F7) | `-mfpu=fpv5-d16 -mfloat-abi=hard` |
| Cortex-M0, M0+, M3 (no FPU) | None. Empty the list. |

This setup is specific to STM32 with CMSIS. A Teensy uses PlatformIO's `teensy` platform, which has different build scripts, so don't assume these flags carry over.

## Check a board is really running

A successful flash doesn't prove the code runs. OpenOCD can briefly halt the chip and read its registers, with no extra hardware. Each command needs its own `-c`, because OpenOCD doesn't print `mdw` results when commands are chained with semicolons in one `-c`.

```powershell
$oc = "$env:USERPROFILE\.platformio\packages\tool-openocd"
& "$oc\bin\openocd.exe" -s "$oc\openocd\scripts" -f board/st_nucleo_f4.cfg `
    -c init -c halt `
    -c "mdw 0x40020014" `
    -c "mdw 0xE000ED88" `
    -c "mdw 0xE000ED28" `
    -c resume -c shutdown
```

| Address | Register | Healthy value |
|---|---|---|
| `0x40020014` | GPIOA output | Bit 5 (`0x20`) changes between reads while LD2 blinks |
| `0xE000ED88` | CPACR, FPU access | `0x00F00000` means the FPU is enabled |
| `0xE000ED28` | CFSR, fault status | `0x00000000` means no faults |

## Problems and fixes

These all happened during this setup.

| Symptom | Cause | Fix |
|---|---|---|
| Link error: `uses VFP register arguments, firmware.elf does not` | FPU flags reached the compiler but not the linker | Add them to `LINKFLAGS` in `build_flags.py` |
| `cc1.exe: warning: command line option '-fno-rtti' is valid for C++/ObjC++ but not for C` | `build_flags` also applies to C files | Remove the repeated flag, and put C++-only flags in `CXXFLAGS` |
| `SystemCoreClock` reads 562.5 MHz and timing runs about 3 times slow | CMSIS assumes a 25 MHz HSE, but the Nucleo supplies 8 MHz | `-DHSE_VALUE=8000000U` |
| Mirror connection refused while installing packages | One PlatformIO mirror was unreachable | None needed. It retried another mirror. |
| OpenOCD `mdw` prints nothing | Commands were chained with `;` inside one `-c` | Give each command its own `-c` |
| `pio debug` with extra arguments says "No stack" and can't find symbols | Your arguments replaced PlatformIO's init file | Include `-x .pioinit` |

**`F_CPU` is misleading:** PlatformIO defines `F_CPU=180000000L` from the board's data, but the chip starts at 16 MHz on its internal oscillator and stays there until your code changes it. Use `SystemCoreClock` after calling `SystemCoreClockUpdate()`, not `F_CPU`.

## What's been tested

Tested on Windows 11 with a NUCLEO-F446RE on 2026-11-05:

| Check | Result |
|---|---|
| Clean firmware build | No warnings, and the ELF reports hard-float (`Tag_ABI_VFP_args: VFP registers`) |
| PC tests | Pass, with `__cplusplus` at `201103` |
| Flash | `** Verified OK **` |
| Running on the board | PA5 toggles, FPU enabled, no faults |
| 180 MHz clock (throwaway test) | `SystemCoreClock` reads 180000000, and SysTick runs at about 1 kHz |
| GDB from the terminal | Connects, resets, sets a breakpoint, steps and reads registers |

Not tested yet:

- A clean install on a second device, including the MSYS2, Linux and macOS steps
- Starting a debug session from the VS Code panel
