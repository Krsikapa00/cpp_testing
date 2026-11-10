# Project 2: FreeRTOS wrappers and UART logger

**Goal:** move the firmware onto FreeRTOS with statically allocated tasks, wrap the kernel objects you'll use in every later project in small C++ classes, and give the board a log you can read on your PC. Two tasks log over USART2 at the same time without blocking each other.

**Why it matters:** every later project runs as tasks that talk through queues and log what they do. The wrappers are where most of the C++ in this plan lives: templates, RAII, non-copyable types and routing an interrupt to an object. The logger is your main debugging tool from Project 3 on.

## Already set up

The kernel build is scaffolded and was tested on your board with a throwaway program. Your `src/main.cpp` doesn't use it yet, so your current build is unchanged until you include `FreeRTOS.h`.

| File | What it is |
|---|---|
| `lib/freertos/kernel/` | The official [FreeRTOS-Kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel), a git submodule pinned to V11.3.3 |
| `lib/freertos/library.json` | Tells PlatformIO to build the core files and the `GCC/ARM_CM4F` port, with no `heap_x.c`, and to find `FreeRTOSConfig.h` in `include/` |
| `include/FreeRTOSConfig.h` | The kernel configuration. Read it once: each setting has a comment saying why it's there. |

The configuration follows the [plan's FreeRTOS setup](../PLAN.md#freertos):

| Setting | Value | What it means for you |
|---|---|---|
| Static allocation only | `configSUPPORT_DYNAMIC_ALLOCATION 0` | `xTaskCreate` and `xQueueCreate` don't exist, so use the `...Static` versions |
| Tick | 1000 Hz from `SystemCoreClock` | 1 tick = 1 ms. Call `hal::clock::init()` before starting the scheduler. |
| Handler names | `SVC_Handler`, `PendSV_Handler`, `SysTick_Handler` | The kernel owns all three. Your `SysTick_Handler` has to go. |
| Interrupt priorities | Kernel at 15, `FromISR` limit at 5 | Any ISR that calls a `FromISR` function needs priority 5 to 15 |
| Safety nets | `configASSERT`, stack overflow check 2, handler installation check | A failed check stops the board with interrupts off. Halt it in the debugger and read the call stack. |
| Timers | Off | Turn on later if you need them, and then provide `vApplicationGetTimerTaskMemory()` |

**Test results on your board:** two static tasks ran. `xTickCount` advanced 2006 in 2 s, a 500 ms `xTaskDelayUntil` task ran 4 times, and a 10 ms `vTaskDelay` task ran 201 times. `SHPR3` read `0xF0F00000`, which puts PendSV and SysTick at priority 15. `CONTROL` read `0x02`, so the tasks ran on their own stacks. There were no faults, and no build warnings even with the kernel compiled under your `-Wall -Wextra -Wpedantic`.

## Before you start: move Project 1 onto the kernel

1. **Remove your tick.** Delete `SysTick_Handler`, `msCounter` and the `SysTick_Config()` call. The kernel sets SysTick up itself when the scheduler starts. If you keep your handler, the link fails with a duplicate `SysTick_Handler`.
2. **Use the kernel's time.** `getTimeMs()` becomes `xTaskGetTickCount()`, which uses the same `now - last` wraparound rule.
3. **Write the hooks the configuration asks for.** Put them in their own file, and declare all three `extern "C"`:
   - `vApplicationGetIdleTaskMemory()` hands the kernel a buffer for the idle task's `StaticTask_t` and a stack of `configMINIMAL_STACK_SIZE` words. Copy the signature from `task.h`. In V11 the last parameter is `configSTACK_DEPTH_TYPE *`.
   - `vApplicationStackOverflowHook()` runs when a task overflows its stack. Stop there with interrupts disabled, like `configASSERT` does.
   - `_sbrk()` is what `malloc` calls to grow the heap. Write your own that stops the board, so any hidden heap use stops immediately instead of quietly working.
4. **Blink LD2 from a task** using `xTaskDelayUntil()`, then start the scheduler with `vTaskStartScheduler()`. Nothing after that call runs.
5. **Set the button's interrupt priority.** It's 0 now, the NVIC default. That's fine while the handler only sets an `std::atomic`. Once it calls a `FromISR` function, for example to notify a task, it must be 5 or more.

## What to build

### 1. `Task<StackWords>` in `lib/rtos/`

- A class template that owns its stack (`std::array<StackType_t, StackWords>`) and its `StaticTask_t`, so a task and its memory are one object
- A `start()` function that takes an entry function, a `void*` context, a name and a priority, and calls `xTaskCreateStatic()`
- The constructor must not call the kernel. Task objects are usually globals, and global constructors run before `main` (Project 1's experiment 4).
- Not copyable or movable, using `= delete`. The kernel holds pointers into the object.
- A `static_assert` that the stack is at least `configMINIMAL_STACK_SIZE`

To run a member function as a task, the usual C++ pattern is a static member function that casts the context back to the object:

```cpp
static void entry(void* self) { static_cast<Blinker*>(self)->run(); }
```

The plan uses this function pointer and context approach, not a base class with a virtual `run()`, to keep to composition.

### 2. `Queue<T, N>`

- Owns `StaticQueue_t` and the storage for `N` items of `T`
- `send()` and `receive()` with a timeout in ticks, returning `bool`. Also `sendFromIsr()`, which handles `pxHigherPriorityTaskWoken` and calls `portYIELD_FROM_ISR()`.
- A `static_assert` that `T` is trivially copyable. The kernel copies items with `memcpy`, so a type with a constructor or destructor would silently break.

### 3. `Mutex` and `ScopedLock`

- `Mutex` wraps `xSemaphoreCreateMutexStatic()`. It's not copyable.
- `ScopedLock` takes the mutex in its constructor and gives it back in its destructor, so every return path unlocks. This is RAII, the main reason C++ is nicer than C for this.
- Mutexes can't be used from an ISR. A mutex knows which task owns it, and that's how priority inheritance works.

### 4. `RingBuffer<T, N>` in `lib/control/`

- A single-producer, single-consumer ring buffer. In the logger, the producer is whichever task holds the mutex, and the consumer is the UART interrupt.
- Head and tail as `std::atomic` indices, with no locks and no FreeRTOS. It must build on the PC too.
- `push()` returns `false` when full and `pop(T&)` returns `false` when empty. Never block, and never overwrite.
- A `static_assert` that `N` is a power of two, so wrapping is a mask. With free-running indices, `head - tail` is the number of items even after the indices overflow. It's the same rule as `now - last`.
- **PC tests** in `test/test_ring_buffer/`, run with `pio test -e native`: empty, full, one item, wrapping around the end of the array many times, and refusing a push when full.

### 5. `Uart` driver for USART2 in `lib/drivers/`

USART2 reaches your PC through the ST-LINK as COM3 on Windows and `/dev/ttyACM0` on Linux.

| Setting | Value |
|---|---|
| Pins | PA2 TX, PA3 RX, alternate function 7 |
| Clock | `RCC_APB1ENR_USART2EN`. APB1 runs at 45 MHz. |
| Baud | 115200. `BRR` = 45 MHz / 115200 = 390.6, so write 391 (`0x187`). The error is 0.1%. |
| Frame | 8 data bits, no parity, 1 stop bit, which is the reset default |
| Interrupt | `USART2_IRQn` (38) at priority 6, for example |

Transmit from the interrupt, not by waiting on the `TXE` flag:

- Writing a line pushes its characters into the ring buffer, then turns on `TXEIE`.
- `USART2_IRQHandler` writes one character to `DR` each time `TXE` is set, and turns `TXEIE` off when the buffer is empty.

The handler is a C function, but the buffer belongs to an object. Getting from one to the other is the design question below.

### 6. `Logger`

- `log(const char*)` copies a whole line into the ring buffer while holding the mutex, so lines from different tasks never mix.
- If the line doesn't fit, drop it and count the drop. Never wait for the UART, so a slow serial port can't stall a task.
- Write small helpers to format integers. See experiment 1 for why not `printf`.

### 7. Two tasks logging

For example, one task logs the tick count every 10 ms and another logs a short message every 500 ms, at different priorities. Watch the output with `pio device monitor`.

## Design question

The UART interrupt handler has to be `extern "C" void USART2_IRQHandler()`, with no arguments, but the ring buffer it drains lives inside an object. Try at least two ways of connecting them:

- **A static pointer:** the object registers itself in `init()`, and the handler calls `instance->onInterrupt()`
- **A template on the peripheral address**, like `Gpio`, where everything is static and there's no object at all
- Your own idea

For each, consider:
- What happens if the interrupt fires before `init()`?
- Could the same class drive USART1 and USART2?
- What does the handler compile to?

## Experiments

1. **Find hidden heap use.** Call `snprintf(buf, sizeof buf, "%u", n)` once, rebuild, and check `nm` for `_malloc_r` and `_sbrk`. Both get linked, even for a plain integer. In the test build, `%u` didn't actually call `malloc` at run time. Your `_sbrk` trap is what tells you if a path ever does. Then try `printf`, which writes to a stream.
2. **Copy a `Mutex`.** Read the error that `= delete` gives you, and compare it with what happens in C when two copies of a handle are both used.
3. **Make a `Queue<T, N>` of a type with a constructor**, and check that your `static_assert` message explains the problem.
4. **Call a `FromISR` function from an ISR at priority 0.** `configASSERT` stops the board inside `vPortValidateInterruptPriority()`. Halt in the debugger and find the line. This is the most common FreeRTOS mistake on Cortex-M.
5. **Give a task a 64-word stack** and a large local array. Check that your stack overflow hook runs, then use `uxTaskGetStackHighWaterMark()` to size the stack properly.

## How to check it

| Check | Expected |
|---|---|
| Serial output at 115200 baud | Lines from both tasks, none mixed together, and the drop count at 0 |
| `xTickCount` (from `nm`) | Advances about 1000 per second |
| `SHPR3` (`0xE000ED20`) | `0xF0F00000`, with PendSV and SysTick at 15 |
| `USART2_BRR` (`0x40004408`) | `0x00000187` |
| `USART2_CR1` (`0x4000440C`) | `UE` and `TE` set (`0x2008`), plus `RE` (`0x0004`) if you receive. `TXEIE` (`0x0080`) is set only while sending. |
| `GPIOA_MODER` (`0x40020000`) | `0xA80004A0`: debug pins kept, PA5 an output, PA2 and PA3 alternate function |
| `GPIOA_AFRL` (`0x40020020`), bits 15:8 | `0x77`, which is AF7 on PA2 and PA3 |
| `NVIC_ISER1` (`0xE000E104`) | Bit 6 (USART2) and bit 8 (button) set: `0x00000140` |
| USART2 priority, `0xE000E424` bits 23:16 | Your priority, shifted left 4. Priority 6 is `0x60`. |
| `CFSR` and `HFSR` | `0` |
| `nm` | Either no `_malloc_r`, or your `_sbrk` trap is never reached |

When you get there, I'll update both `board-check` scripts. `TICK_SYMBOL` becomes `xTickCount`, and the USART2 and priority checks get added.

## Done when

- [ ] Two tasks log over USART2 without blocking each other
- [ ] `_sbrk` is never called
- [ ] `RingBuffer` passes PC unit tests
- [ ] `Queue<T, N>` rejects non-trivially-copyable types with a `static_assert`
- [ ] `pio run` gives no warnings, and `pio test -e native` passes
- [ ] You've done the five experiments and answered the design question

## Suggested order

1. Move Project 1 onto the kernel, including the hooks and a blink task, then run `board-check`.
2. Write `Task`, and run the blink through it.
3. Write `RingBuffer` and its PC tests. This needs no hardware, so it's a good thing to do away from the board.
4. Write `Uart`, and send a fixed string from the interrupt.
5. Write `Mutex`, `ScopedLock` and `Logger`, then add the second task.
6. Write `Queue`. Moving the button press to a task through it is a good first use, and it needs the button's priority set.

## Next

Project 3 adds `PwmChannel`, `Servo` and a fixed-rate control task on `xTaskDelayUntil`, and logs its timing jitter through this logger.
