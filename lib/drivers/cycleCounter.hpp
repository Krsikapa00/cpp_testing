#pragma once

// The Cortex-M4's DWT cycle counter: a free-running 32-bit count of CPU cycles, for timing code
// to the cycle. At 180 MHz one cycle is 5.6 ns, and the count wraps every 23.9 s. Differences
// taken with unsigned subtraction (`now - then`) stay correct across the wrap, the same rule
// as the tick count, as long as the interval is under 23.9 s.
//
// It's part of the debug unit, so it needs TRCENA in DEMCR first. A debugger sets that itself,
// which is how code that forgets init() works on the bench and then reads 0 in the field.
// (On the Teensy's Cortex-M7, DWT->LAR must be unlocked too. The M4 has no lock.)

#include "stm32f4xx.h"
#include <cstdint>

namespace hal {

class CycleCounter {
public:
    CycleCounter() = delete;

    static void init() {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }

    static std::uint32_t now() { return DWT->CYCCNT; }

    // Conversions for logging. SystemCoreClock is 180000000 after hal::clock::init().
    static std::uint32_t toUs(std::uint32_t cycles) { return cycles / (SystemCoreClock / 1000000u); }
    static std::uint32_t toNs(std::uint32_t cycles) {
        // In 64 bits, because cycles * 1000 overflows 32 bits for anything over about 24 ms
        return static_cast<std::uint32_t>((static_cast<std::uint64_t>(cycles) * 1000u)
                                          / (SystemCoreClock / 1000000u));
    }
};

} // namespace hal
