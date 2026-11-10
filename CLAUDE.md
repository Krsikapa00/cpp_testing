# CLAUDE.md

Prep repo for a drone flight-controller role: learning embedded C++11 on a NUCLEO-F446RE, standing in for the company's Teensy 4.1. The full plan is in [PLAN.md](PLAN.md), with ground rules, projects and C++11 traps. The company's stack is in [stackDescription.md](stackDescription.md).

**Device handoff:** work moves between a Windows machine and a Linux machine, both using VS Code. This file is the handoff between them. Claude's memory on one machine doesn't reach the other, so keep [Current state](#current-state) up to date at the end of each session.

## Start here on a new device

1. **Set up the environment** following [docs/environment-setup.md](docs/environment-setup.md): VS Code with the PlatformIO IDE extension, a host `g++`, and USB access to the ST-LINK. On Linux, that means the PlatformIO udev rules.
2. **Get the latest work** with `git pull`. The check script also runs `git submodule update --init --recursive`, which matters from Project 2.
3. **Run the setup check** with the board plugged in:

   | OS | Command |
   |---|---|
   | Windows | `powershell -ExecutionPolicy Bypass -File scripts/check-setup.ps1 -Board` |
   | Linux | `bash scripts/check-setup.sh --board` |

   Every line should say `PASS`. Git says `WARN` when the branch is ahead or behind its remote, so pull or push before switching devices.

## Scripts

Each script has a Windows (`.ps1`) and Linux (`.sh`) version that behave the same, and both share their path logic through `tools.ps1` or `tools.sh`. Run the Linux versions with `bash`, as the executable bit doesn't survive git from Windows. On Windows, use `powershell -ExecutionPolicy Bypass -File`, because new installs block `.ps1` scripts.

| Script | What it does | When to use it |
|---|---|---|
| `check-setup` | Checks git, PlatformIO and `g++`, does a clean build that fails on warnings, and runs the PC tests. With `-Board` or `--board`, it also flashes and runs the board check. | After switching devices or pulling |
| `board-check` | Reads registers through the ST-LINK and compares them with the current project's expected values. Also checks the tick counter advances about 5000 and LD2 changes over 5 s. | After flashing, to prove the board really runs the code |
| `listing` | Builds, then writes the vector table and disassembly with source to `.pio/build/nucleo_f446re/firmware.lst`, and opens it in VS Code | Reading the generated code |

**When the expected board state changes**, for example a new clock setting, LED behaviour or peripheral, update the checks in **both** `board-check.ps1` and `board-check.sh`.

## Current state

*Last updated 2026-11-09, on Windows.*

**Project 1: Bring-up and GPIO** ([brief](docs/project-1-brief.md)), in progress:

| Part | Status |
|---|---|
| 1. `Gpio` template ([lib/drivers/gpio.hpp](lib/drivers/gpio.hpp)) | Done and reviewed. `MODER` reads `0xA8000400` on the board. |
| 2. 180 MHz clock ([lib/drivers/clock.cpp](lib/drivers/clock.cpp)) | Done, reviewed and checked on the board. `SystemCoreClock` reads 180000000, and every register matches. |
| 3. 1 kHz SysTick tick (in [src/main.cpp](src/main.cpp)) | Working on the board, with `getTimeMs()`. `SYST_RVR` reads `0x0002BF1F`, and `msCounter` advances about 5000 in 5 s. One fix pending, below. |
| 4. Button interrupt (PC13, `EXTI15_10_IRQn`, in [src/main.cpp](src/main.cpp)) | Working on the board, reviewed 2026-11-09. `EXTICR4` reads `0x20`. A falling edge driven on PC13 through the debugger switched LD2 from 1000 ms to 500 ms and back, and a second edge 10 ms later was ignored. Small fixes pending, below. |
| Design question (template vs ordinary class) | Not started |
| Five experiments in the brief | Experiment 1 (missing `extern "C"`) happened by accident on 2026-11-09 and was diagnosed. The other four aren't started. |

**Review fixes pending for the tick and button (2026-11-09):**
- Remove the `if (now < last)` correction in the LED loop. Unsigned `now - last` already handles wraparound, and adding `0xFFFFFFFF` is the same as subtracting 1, so it's 1 ms late at the wrap. The wrap itself was tested on the board by writing `0xFFFFF000` to `msCounter`: the blink stayed at 1 s across zero.
- In `configureBtn()`, the line that clears `EXTICR[3]` masks `EXTI15` but then sets `EXTI13_PC`. It only works because EXTI13's field resets to 0. Clear `SYSCFG_EXTICR4_EXTI13` instead, and fix the comment above it.
- Read `RCC->APB2ENR` back after enabling the SYSCFG clock, as with the GPIO clocks.

**Testing the button without pressing it:** halt, write `0x04000000` to `GPIOC_MODER` (`0x40020800`) so PC13 drives low, then write `0` to make it an input again. B1's pull-up limits the current. `EXTI_PR` then reads `0x2000`, and the interrupt runs on resume.

**Small open items from earlier reviews:**
- In `clock.cpp`, include `clock.hpp` first, and drop the unneeded `extern` in `clock.hpp`.
- Optional: timeouts with an `enum class` status on the clock's ready-flag loops, and `constexpr` and `static_assert` checks on the PLL settings.
- `firmware.lst` in the repo root is an old listing. The `listing` script now writes to `.pio/`, which git ignores.

**Parts:** whether the IMU, 5 V servo supply and logic analyzer have been ordered isn't recorded yet. Project 3 needs the supply, and Project 4 the IMU.

**Timeline:** started 2026-11-05, and the role starts around 2026-12-05. The goal was to finish Projects 1 and 2 in the first week, ending around 2026-11-12, then about 10 hours a week after that.

## How to work with me

- **I write the project code.** Claude reviews it, explains it and writes briefs, docs and tooling, but doesn't write project code unless I ask.
- **Reviews:**
  - Start from a clean build (`pio run -t clean`, then `pio run`), because PlatformIO only prints warnings for files it recompiles.
  - Compile edge cases the current code doesn't reach in a scratch file, and read the generated assembly.
  - Flash the board and read registers to confirm what it's actually doing.
  - List findings most important first, with file and line links, and say what wasn't checked.
- **Never claim the board works without reading its registers.** A blink that looks right has hidden a wrong clock before.
- **Don't commit or push** unless I ask.
- **Keep docs in sync:** this file's [Current state](#current-state), the progress boxes in `PLAN.md`, and both `board-check` scripts.

## Ground rules

From [PLAN.md](PLAN.md#ground-rules):
- C++11 only, built with `-std=gnu++11`.
- Classes and namespaces, with composition over inheritance.
- No heap.
- `framework = cmsis`, with hand-written drivers and no Arduino libraries.
- FreeRTOS from Project 2, statically allocated.

## Layout

| Path | Contents |
|---|---|
| `src/main.cpp` | Application entry point |
| `lib/drivers/` | `gpio.hpp` (`hal::Gpio<PortBase, Pin>`), `clock.hpp` and `clock.cpp` (`hal::clock::init()`) |
| `test/test_toolchain/` | PC test that confirms C++11 mode. Real tests start in Project 2. |
| `scripts/` | `build_flags.py` (PlatformIO extra script), plus the scripts above |
| `docs/` | `environment-setup.md` and `project-1-brief.md` |
| `disassemble.md` | My notes on reading listings |

## Commands

| Task | Command |
|---|---|
| Build | `pio run` |
| Clean build, showing all warnings | `pio run -t clean`, then `pio run` |
| Flash | `pio run -t upload` |
| PC tests | `pio test -e native` |
| Debug from the terminal | `pio debug --interface=gdb -- -x .pioinit` |
| Serial monitor at 115200 baud | `pio device monitor` |

If `pio` isn't on `PATH`, use the PlatformIO Core CLI terminal in VS Code. On Linux it's at `~/.platformio/penv/bin/pio`.

## Traps found so far

These all happened in this repo. The setup guide has a full [problems and fixes](docs/environment-setup.md#problems-and-fixes) table.

- **CMSIS assumes a 25 MHz HSE.** `-DHSE_VALUE=8000000U` in `platformio.ini` fixes it. Without it, `SystemCoreClock` reads 562.5 MHz and timing runs about 3 times slow.
- **`F_CPU` is misleading.** It's always 180 MHz, but the chip starts at 16 MHz. Use `SystemCoreClock`.
- **Hard-float flags must reach the linker.** `scripts/build_flags.py` handles that.
- **Warnings disappear on incremental builds.** A `!=` that should have been `|=` hid this way, and its warning only showed on a clean build.
- **`PLLCFGR` has a non-zero reset value**, so write it with `=`. The P field is encoded: ÷2 is written as `0`.
- **C++17 features can slip through.** Arm GCC 7.2 accepts nested namespaces (`namespace a::b`) with only a warning, but the host's GCC 5.4 rejects them. Some C++17 features, like `[[nodiscard]]`, compile with no warning at all.
- **Peripheral clocks need a delay.** After enabling one, read the `RCC` register back before touching the peripheral. On GPIOA, a bad read-modify-write of `MODER` could cut off the debug pins.
- **A missing `extern "C"` on an interrupt handler fails silently.** The vector table entry stays at `Default_Handler` (`0x08000255`).
- **OpenOCD output:** give each command its own `-c`, or `mdw` prints nothing.
- **`pio debug` with extra arguments** needs `-x .pioinit` included.

## Hardware notes

- **Board:** NUCLEO-F446RE, a Cortex-M4F with a single-precision FPU, 512 KB flash and 128 KB RAM.
- **Programmer:** the onboard ST-LINK/V2-1. Its virtual serial port is USART2 on PA2/PA3. The port name differs by device: COM3 on the Windows machine, and usually `/dev/ttyACM0` on Linux.
- **Pins:** LD2 is on PA5, which is also SPI1's clock, so use SPI2 on PB13–PB15 for the IMU. B1 is on PC13 and reads low when pressed.
- **Clock source:** HSE is the 8 MHz signal from the ST-LINK, used in bypass mode.
