# Project 1: Bring-up and GPIO

**Goal:** replace the toolchain check in `src/main.cpp` with a clean C++ program that runs at 180 MHz, blinks LD2 from a 1 kHz tick and toggles a mode with the button. There's no RTOS yet.

**Why it matters:** this is the base for every later project, and it covers the most common C-to-C++ traps while the hardware is still simple.

## What to build

### 1. `Gpio` template in `lib/drivers/gpio.hpp`

- A class template, `Gpio<PortBase, Pin>`, used like `using Led = Gpio<GPIOA_BASE, 5>;`
- Functions to enable the port clock, set the pin mode, set, clear, toggle and read
- A `static_assert` that rejects invalid pin numbers
- Writes through `BSRR` for set and clear, because it can't race with an interrupt the way a read-modify-write of `ODR` can
- A read-back after enabling a peripheral clock, because the chip needs a short delay before the peripheral is usable
- Everything in a namespace, with `enum class` for pin modes

PlatformIO will find it with `#include "gpio.hpp"` and no extra configuration.

### 2. Clock setup to 180 MHz

These settings and this order were tested on your board. Read the RCC and PWR chapters of RM0390 to see why each step is needed, then write it yourself.

| Setting | Value |
|---|---|
| HSE | Bypass mode, 8 MHz from the ST-LINK |
| PLL | M = 8, N = 360, P = 2, Q = 7, R = 2 |
| Flash | 5 wait states, with prefetch and both caches on |
| Bus prescalers | AHB /1 (180 MHz), APB1 /4 (45 MHz), APB2 /2 (90 MHz) |

1. Enable the PWR clock and select voltage scale 1.
2. Turn on HSE with bypass, then wait until it's ready.
3. Configure the PLL and turn it on.
4. Enable over-drive, waiting for each of its two ready flags.
5. Wait for the PLL to be ready.
6. Set flash wait states, **before** speeding up.
7. Set the bus prescalers, switch to the PLL and wait for the switch to take effect.
8. Call `SystemCoreClockUpdate()`.

### 3. 1 kHz tick

- Use `SysTick_Config()` and a `std::atomic<std::uint32_t>` counter
- Provide a small function for reading the time in milliseconds
- Handle counter wraparound correctly by comparing with `now - last >= period`

### 4. Button interrupt

- B1 is on PC13 and reads low when pressed, so trigger on the falling edge. It uses `EXTI15_10_IRQn`.
- This needs the SYSCFG clock, the EXTI line routed to port C, and the interrupt enabled in the NVIC.
- Clear the pending flag in the handler by writing 1 to `EXTI->PR`.
- Ignore edges within about 50 ms of the last accepted press.
- Make the button switch between two blink rates, so you can see it working.

## Design question

Build `Gpio` as a template first. Then write a second version as an ordinary class that stores `GPIO_TypeDef* port_` and `std::uint8_t pin_`, and compare them:

- How much flash and RAM does each use?
- What does a `set()` call compile to in each?
- Which would you use if a driver had to take a pin chosen at runtime?

Keep your conclusions, as we'll discuss them in review. Your team's real code may use either style.

## Experiments

Each takes a few minutes, and they're the fastest way to make these rules stick.

1. **Remove `extern "C"`** from `SysTick_Handler` and rebuild. It still builds with no warning. Check with `nm`: `SysTick_Handler` now has the same address as `Default_Handler`, so your tick never runs.
2. **Declare `constexpr GPIO_TypeDef* p = GPIOA;`** to see the error. This is why the template takes `GPIOA_BASE`, an integer, and only casts it where it's used.
3. **Compare with C.** Write the equivalent register writes by hand and compare `objdump -d -S` output with your `Gpio` version.
4. **Add a global object whose constructor touches a register.** Set a breakpoint in it and look at the call stack and `SystemCoreClock`. It runs before `main`, so your clock setup hasn't happened yet.
5. **Remove `-DHSE_VALUE=8000000U`** and rebuild. The blink runs about 3 times too slow with no error, which is why you should always measure timing.

## How to check it

The exact values below came from a test build on your board. See [environment-setup.md](environment-setup.md#check-a-board-is-really-running) for how to read registers.

| Check | Expected |
|---|---|
| `RCC_CFGR` (`0x40023808`), low byte | `0x0A`, meaning the PLL is selected and active |
| `RCC_PLLCFGR` (`0x40023804`) | `0x27405A08` |
| `FLASH_ACR` (`0x40023C00`) | `0x00000705` |
| `SystemCoreClock` in the debugger | `180000000` |
| Fault status `CFSR` (`0xE000ED28`) | `0x00000000` |
| LED blinking at 1 Hz | 60 blinks in 60 seconds on a stopwatch, or a logic analyzer reading |
| Button | Blink rate changes once per press |

**Note:** the test build covered the clock, tick and `Gpio`. Its interrupt setup was loaded but the button was never pressed, so that part is unproven until you try it.

## Done when

- [ ] All the checks above pass
- [ ] `pio run` gives no warnings
- [ ] You've done the five experiments
- [ ] You've answered the design question
- [ ] `src/main.cpp` contains only setup and the main loop, with no direct register access for GPIO
- [ ] The toolchain check test is still passing

## Review

When you're done, or stuck, ask for a review. It'll check against the [ground rules](../PLAN.md#ground-rules), the traps in the plan, and how readable your interfaces are. Commit working steps as you go, so it's easy to compare your experiments with a known good build.

## Next

Project 2 brings in FreeRTOS, which takes over SysTick. Keep the tick code simple, as it'll be replaced.
