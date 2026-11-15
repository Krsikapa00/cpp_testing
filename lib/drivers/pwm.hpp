#pragma once

// PWM output from a general-purpose timer, in two layers:
//
//     using ServoTimer = hal::PwmTimer<hal::Tim3, 20000>;                          // 50 Hz, 1 us steps
//     using ServoPwm   = hal::PwmChannel<ServoTimer, 1, hal::Gpio<GPIOA_BASE, 6>, 2>; // TIM3_CH1 on PA6, AF2
//     ServoTimer::init();
//     ServoPwm::init(1500_us);
//     ServoPwm::setPulse(1200_us);
//
// PwmTimer owns the counter: clock, prescaler and period. One timer can serve up to four
// channels with the same period, for example four servos on TIM3. PwmChannel owns one compare
// channel and its pin. Everything is static and templated, like Gpio and UartStatic.
//
// The counter always ticks at 1 MHz, so pulse widths are whole microseconds and CCR holds the
// width directly. That suits servos (1000 to 2000 us in a 20000 us period). A DC motor at 20 kHz
// would get only 50 steps, so it would need a faster tick: make TickHz a template parameter then.
//
// Pin conflicts on this board: PA5 is LD2 and PA2/PA3 are USART2, so they're not used for PWM.

#include "stm32f4xx.h"
#include "gpio.hpp"
#include "units.hpp"
#include <cstdint>

namespace hal {

// Per-timer facts. TIM4 would be another of these, also on APB1.
struct Tim3 {
    static constexpr std::uintptr_t base    = TIM3_BASE;
    // APB1 runs at 45 MHz, but because APB1's prescaler isn't 1, the timers on it get twice that.
    // RM0390, "Clock tree": TIMxCLK = 2 x PCLKx when the APB prescaler is more than 1.
    static constexpr std::uint32_t  clockHz = 90000000u;

    static void enableClock() {
        RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
        (void)RCC->APB1ENR;
    }
};

template<typename Timer, std::uint32_t PeriodUs>
class PwmTimer {
    static constexpr std::uint32_t tickHz    = 1000000u;
    static constexpr std::uint32_t prescaler = Timer::clockHz / tickHz - 1u;
    static_assert(Timer::clockHz % tickHz == 0, "Timer clock must be a whole number of MHz");
    static_assert(prescaler <= 0xFFFFu, "Prescaler doesn't fit in 16 bits");
    static_assert(PeriodUs >= 2u && PeriodUs - 1u <= 0xFFFFu, "Period must fit TIM3's 16-bit counter");

public:
    PwmTimer() = delete;

    static constexpr std::uint32_t periodUs = PeriodUs;

    static TIM_TypeDef& regs() { return *reinterpret_cast<TIM_TypeDef*>(Timer::base); }

    static void init() {
        Timer::enableClock();
        TIM_TypeDef& t = regs();
        t.CR1 = 0;                        // stopped while configuring
        t.PSC = prescaler;                // 90 MHz / 90 = 1 MHz
        t.ARR = PeriodUs - 1u;            // counts 0 .. PeriodUs-1, so the period is PeriodUs ticks
        t.CR1 = TIM_CR1_ARPE;             // ARR changes take effect at the next period, not mid-period
        t.EGR = TIM_EGR_UG;               // load PSC and ARR from their preload registers now
        t.CR1 |= TIM_CR1_CEN;
    }
};

template<typename PwmTimerT, unsigned Channel, typename Pin, std::uint8_t Af>
class PwmChannel {
    static_assert(Channel >= 1 && Channel <= 4, "Timer channels are 1 to 4");
    static_assert(Af <= 15, "Alternate function is 0 to 15");

    // CCMR1 holds channels 1 and 2, CCMR2 holds 3 and 4, 8 bits each.
    static constexpr unsigned ccmrShift = ((Channel - 1u) % 2u) * 8u;
    static constexpr unsigned ccerShift = (Channel - 1u) * 4u;

public:
    PwmChannel() = delete;

    // Sets the first pulse before turning the output on, so the first frame is already right.
    static void init(units::Microseconds initialPulse) {
        TIM_TypeDef& t = PwmTimerT::regs();
        setPulse(initialPulse);

        // PWM mode 1 (OCxM = 110: high while CNT < CCR) with preload, so a new width takes effect
        // at the start of the next period and never cuts a pulse short.
        const std::uint32_t mode = (6u << TIM_CCMR1_OC1M_Pos) | TIM_CCMR1_OC1PE;
        volatile std::uint32_t& ccmr = (Channel <= 2) ? t.CCMR1 : t.CCMR2;
        ccmr = (ccmr & ~(0xFFu << ccmrShift)) | (mode << ccmrShift);

        // Select the alternate function before switching the pin to it, as for the UART.
        Pin::enableClock();
        Pin::setAF(Af);
        Pin::setMode(PinMode::Alternate);

        t.CCER |= (TIM_CCER_CC1E << ccerShift);    // active high, output on
    }

    // Widths longer than the period are clamped to it, which means always high.
    static void setPulse(units::Microseconds width) {
        std::uint32_t w = width.count();
        if (w > PwmTimerT::periodUs) {
            w = PwmTimerT::periodUs;
        }
        ccr() = w;
    }

    // What's in the compare register now, so what the next period will send.
    static units::Microseconds pulse() { return units::Microseconds(ccr()); }

private:
    // A switch on a template parameter: the compiler keeps only the matching case.
    static volatile std::uint32_t& ccr() {
        TIM_TypeDef& t = PwmTimerT::regs();
        switch (Channel) {
            case 1:  return t.CCR1;
            case 2:  return t.CCR2;
            case 3:  return t.CCR3;
            default: return t.CCR4;
        }
    }
};

} // namespace hal
