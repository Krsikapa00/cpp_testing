# Embedded C++ Prep Plan

**Goal:** move from embedded C to embedded C++ before starting a drone flight-controller role, by building one firmware that grows from a blinking LED into a self-levelling servo platform with the structure of a flight controller.

**Timeline:** four weeks from 2026-11-05, at an assumed 10 hours a week. The role starts by about 2026-12-05.

**Background:** the company's stack is described in [stackDescription.md](stackDescription.md). I mostly write application-level C on an RTOS, using a vendor HAL or existing drivers, with some register-level experience and some non-embedded C++.

## Progress

- [ ] Parts ordered
- [ ] Project 1: Bring-up and GPIO
- [ ] Project 2: FreeRTOS wrappers and UART logger
- [ ] Project 3: Actuators and fixed-rate task
- [ ] Project 4: SPI IMU driver
- [ ] Project 5: Estimator, PID and telemetry
- [ ] Project 6: Self-levelling platform (capstone)

## Ground rules

These match how the company writes code, or stand in for it where the hardware differs.

| Area | Rule |
|---|---|
| Language | C++11 |
| C++ style | Classes and namespaces, composition over inheritance |
| Memory | No heap, including heap-allocating `std` features |
| RTOS | FreeRTOS, statically allocated only. The company uses an RTOS, but which one isn't confirmed. |
| Libraries | No Arduino libraries. `framework = cmsis`, with hand-written drivers. |
| Board | NUCLEO-F446RE, in place of the Teensy 4.1 |
| Tools | PlatformIO in VS Code |

**Board differences:** the F446RE is a Cortex-M4F with a single-precision FPU, while the Teensy 4.1 is a Cortex-M7 with double-precision support. Use `float`, not `double`, and let `-Wdouble-promotion` catch accidental promotions.

**Limitation:** an SG90 is a slow position servo, typically updated at 50 Hz, so the capstone teaches flight-controller structure rather than real drone tuning. Everything stays on the bench, with no propellers.

## Hardware

| Item | Status | Needed for |
|---|---|---|
| NUCLEO-F446RE | Have | Everything. Its onboard ST-LINK handles flashing, debugging and a serial port. |
| SG90 servo | Have | Projects 3 and 6 |
| DC motors | Have | Project 3 (optional) |
| SPI IMU breakout | **Buy, required** | Projects 4–6. Ideally an ICM-42688-P. An MPU-6050 works if shipping is slow, but it's I2C-only, so you'd write an I2C bus instead. |
| Separate 5 V supply | **Buy, required** | Powers the servo. Tie its ground to the Nucleo's. |
| USB logic analyzer | **Buy, strongly recommended** | Debugging SPI and PWM. A cheap 8-channel one works with PulseView. |
| TB6612FNG or DRV8833 | Buy, optional | DC motors, which can't run directly from GPIO pins |

**Pins to watch** (confirm against UM1724 and the datasheet):

- User LED LD2 is on PA5, which is also the default SPI1 clock. Use SPI2 on PB13/PB14/PB15 for the IMU.
- User button B1 is on PC13.
- USART2 on PA2/PA3 reaches your PC through the ST-LINK's virtual serial port.

**References:**

- RM0390: STM32F446 reference manual
- PM0214: Cortex-M4 programming manual
- UM1724: Nucleo-64 board manual
- STM32F446xx datasheet: pin alternate functions

## Setup

### PlatformIO

This is a starting point that hasn't been built yet. Project 1 proves it out and corrects it.

```ini
[platformio]
default_envs = nucleo_f446re

[env:nucleo_f446re]
platform = ststm32@20.0.0
board = nucleo_f446re
framework = cmsis
upload_protocol = stlink
debug_tool = stlink
build_unflags = -std=gnu++14 -std=gnu++17
build_flags =
    -std=gnu++11
    -fno-exceptions -fno-rtti -fno-threadsafe-statics
    -Wall -Wextra -Wpedantic -Wdouble-promotion
    -mfpu=fpv4-sp-d16 -mfloat-abi=hard

[env:native]
platform = native
test_framework = unity
build_flags = -std=gnu++11 -Wall -Wextra -Wpedantic
```

To check in Project 1:

- **C++ standard:** a verbose build shows that `-std=gnu++11` is the only standard flag in use.
- **Hard float:** the FPU flags reach the linker as well as the compiler, which may need a small PlatformIO extra script. The FreeRTOS `ARM_CM4F` port won't compile without hard float.

### FreeRTOS

The registry's FreeRTOS packages are old or third-party, so use the official kernel:

- **Source:** add [FreeRTOS-Kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel) as a git submodule pinned to a release tag, under `lib/freertos/kernel`, with a `library.json` beside it selecting the core files and the `GCC/ARM_CM4F` port.
- **No heap:** set `configSUPPORT_STATIC_ALLOCATION` to 1 and `configSUPPORT_DYNAMIC_ALLOCATION` to 0, and leave out every `heap_x.c`. Provide `vApplicationGetIdleTaskMemory`, plus `vApplicationGetTimerTaskMemory` if timers are on.
- **Safety nets:** define `configASSERT`, set `configCHECK_FOR_STACK_OVERFLOW` to 2, and implement the overflow hook.
- **Handler names:** map `vPortSVCHandler`, `xPortPendSVHandler` and `xPortSysTickHandler` to the CMSIS names `SVC_Handler`, `PendSV_Handler` and `SysTick_Handler`.
- **Heap ban:** make `_sbrk` trap, so any accidental heap use fails immediately.

### Repo layout

```
lib/
  freertos/      kernel submodule, library.json
  rtos/          Task, Queue, Mutex wrappers
  drivers/       Gpio, Uart, PwmChannel, Spi
  imu/           Imu<Bus>
  control/       RingBuffer, Pid, estimator (portable, tested on PC)
include/         FreeRTOSConfig.h
src/             main.cpp, FlightController
test/            PC unit tests
tools/           Python telemetry and command scripts
```

### Using another device

1. Install VS Code and the PlatformIO extension.
2. Run `git clone --recursive <repo>`. The flag also fetches the FreeRTOS submodule.
3. Run `pio run`, which installs the pinned toolchain.
4. Run `pio test -e native` to run the PC tests. This needs a host `gcc` on `PATH`.

Add `.pio/` to `.gitignore`.

## Projects

### Project 1: Bring-up and GPIO (week 1, no RTOS)

**Build:** PlatformIO setup, 180 MHz clock configuration, a `Gpio<PortBase, Pin>` template, a SysTick tick and a button interrupt.

**C++ focus:** namespaces, `enum class`, `constexpr`, references, `extern "C"`, non-type template parameters.

**Done when:**

- [ ] LD2 blinks at a measured rate, and the button toggles it through an interrupt
- [ ] You've compared the `Gpio` class's disassembly with equivalent C, using `arm-none-eabi-objdump`
- [ ] The map file and `arm-none-eabi-size` output make sense to you

### Project 2: FreeRTOS wrappers and UART logger (weeks 1–2)

**Build:** thin wrappers `Task<StackWords>`, `Queue<T, N>`, `Mutex` and `ScopedLock`, plus an interrupt-driven UART logger using `RingBuffer<T, N>`.

**C++ focus:** templates, RAII, `= delete` for non-copyable types, `std::array`, `std::atomic`, static member functions as task entry points, routing an interrupt to an object.

Wrappers take a function pointer and a context pointer rather than a virtual `run()`, which keeps to composition.

**Done when:**

- [ ] Two tasks log over USART2 without blocking each other
- [ ] `_sbrk` is never called
- [ ] `RingBuffer` passes PC unit tests
- [ ] `Queue<T, N>` rejects non-trivially-copyable types with a `static_assert`

### Project 3: Actuators and fixed-rate task (week 2)

**Build:** `PwmChannel` used by `Servo` and `DcMotor`, and a control task on `xTaskDelayUntil`.

**C++ focus:** composition, strong types for units such as `Degrees`, function pointer and context callbacks instead of `std::function`.

**Done when:**

- [ ] The servo tracks commanded angles, with limits enforced in `Servo`
- [ ] Task timing and jitter are measured with the DWT cycle counter and logged
- [ ] The PWM signal is confirmed on the logic analyzer, if you have one

### Project 4: SPI IMU driver (weeks 2–3)

**Build:** `Imu<Bus>` over SPI2, a mock bus for tests, and a data-ready interrupt that wakes a task through a task notification.

**C++ focus:** compile-time interfaces instead of inheritance, RAII chip select, your own small `Result<T>`, `__attribute__((warn_unused_result))`.

**Done when:**

- [ ] The WHO_AM_I register reads correctly, and raw accel and gyro data look sensible
- [ ] Driver logic passes PC tests against the mock bus
- [ ] Bad reads return an error that the caller is forced to handle

### Project 5: Estimator, PID and telemetry (week 3)

**Build:** a complementary filter, a `Pid` class with integrator limits, and binary telemetry packets with a CRC.

**C++ focus:** `static_assert`, `std::is_trivially_copyable`, `float` vs `double` costs, serializing packets explicitly.

**Done when:**

- [ ] The estimator and PID pass PC tests with known inputs
- [ ] A Python script on your laptop plots live attitude
- [ ] Sharing attitude between tasks never blocks the control task, for example with a one-item queue and `xQueueOverwrite`

### Project 6: Self-levelling platform (weeks 3–4)

**Build:** the IMU sits on a plate driven by the servo, which holds the plate level as you tilt the base. A top-level `FlightController` owns every module.

| Task | Priority | Job |
|---|---|---|
| Control | Highest | Sensor, estimator, PID, mixer, output |
| Command | Medium | Receives "RC" packets from your laptop, standing in for the controller's Teensy |
| Telemetry and log | Lowest | Sends state to your laptop |

**C++ focus:** composing a whole system with no heap and no inheritance, state machines with `enum class`, documenting who owns what.

**Done when:**

- [ ] The plate holds level as the base tilts
- [ ] It won't drive the servo until armed
- [ ] It disarms safely when the command link drops
- [ ] Loop overruns are detected and logged
- [ ] The watchdog resets it if the control task hangs
- [ ] Stack high-water marks are logged for every task, and the sizes are justified

**Stretch goals:** a second axis, or porting to a Teensy to test whether your driver boundaries hold up.

### If you fall behind

Cut in this order:

1. The DC motor driver
2. The Python plotting (log values as text instead)
3. The commands from your laptop and link-loss failsafe

Keep the RTOS wrappers, ring buffer, IMU driver and top-level composition, as they cover most of what the company's notes emphasise.

## C++11 reference

These were compile-tested with `-std=gnu++11` on GCC 5.4 (host) and Arm GCC 7.2 except where noted. The STM32 platform may install a newer Arm GCC, so behaviour could differ slightly.

| Not available or not usable | Use instead |
|---|---|
| `std::optional` | Your own `Result<T>`, or a status code and an out-parameter |
| `std::string_view` | A small pointer-and-length type |
| `std::span` (not compile-tested) | The same, or a pointer and a length |
| `[[nodiscard]]` | `__attribute__((warn_unused_result))` |
| `constexpr` function with several statements | A single `return`, using the ternary operator or recursion |
| Brace-initializing a struct that has default member values | A constructor, or remove the defaults |
| `static_assert` with no message | Always give a message |
| `reinterpret_cast` in a `constexpr` variable, which includes CMSIS macros like `GPIOA` | Keep the address as a `constexpr std::uintptr_t` and cast where it's used |
| `std::enable_if_t` and other `_t`/`_v` helpers (not compile-tested) | `typename std::enable_if<...>::type` and `::value` |
| `std::mutex`, `std::thread` (not compile-tested) | Your FreeRTOS wrappers |

**Safe to use:** `std::array`, `std::atomic`, `<type_traits>`, `<cstdint>`, `enum class`, `nullptr`, `override`, `= delete`, range-based `for`.

### The compiler won't catch everything

- **Newer features can slip through:** Arm GCC 7.2 accepted `[[nodiscard]]` with no warning in C++11 mode, even with `-Wpedantic`. A clean build doesn't prove your code is valid C++11.
- **Compilers disagree:** GCC 5.4 accepted `reinterpret_cast` in a `constexpr` variable, while Arm GCC 7.2 rejected it. Building portable code with both catches more.
- **Debug and release can differ:** using a `static constexpr` member with `std::min` failed to link at `-O0` on the host, but left no undefined symbol at `-O2` on Arm. Add an out-of-class definition such as `constexpr int T::kMax;`.

## Traps

**Moving from C:**

- **Silent interrupt failure:** a handler without `extern "C"` gets a mangled name, so the default handler runs instead of yours, with no build error.
- **Global constructors:** they run before `main`, so touching peripherals in one happens before your clock setup. Use a separate `init()`.
- **Hidden heap use:** `std::function` can allocate, and `std::vector` and `std::string` normally do. Newlib's `printf` family can too.
- **`volatile` isn't enough:** it doesn't make data shared with an interrupt or task safe. Use `std::atomic` or a critical section.
- **Exceptions are off:** `std::array::at()` can't report an error normally, so use a bounds check of your own.

**With FreeRTOS:**

- **Interrupt priorities:** an ISR that calls a `FromISR` function must be at or below `configMAX_SYSCALL_INTERRUPT_PRIORITY`. Priority numbers run backwards, and the default of 0 is too high, so set priorities explicitly. Call `NVIC_SetPriorityGrouping(0)` before starting the scheduler.
- **Function-local statics:** `-fno-threadsafe-statics` removes the guard on their first-call initialization, so two tasks can race. Define shared objects at namespace scope instead.
- **Tick ownership:** FreeRTOS takes over SysTick, so Project 1's tick handler goes away in Project 2.
- **Task stacks:** a large local object can overflow a task's stack, so check high-water marks.
- **Hooks:** C++ implementations of FreeRTOS hooks also need `extern "C"`.

## How we'd work

Claude scaffolds the build setup and writes a short brief for each project. I write the code, and Claude reviews it against the ground rules.

## Questions for the company

- Which RTOS do you use, and do you allocate its objects statically?
- What's the exact `-std` flag and compiler version in the Teensy build?
- Do you use the Arduino core or Teensy libraries on the flight controller?
