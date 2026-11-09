#include "stm32f4xx.h"
#include "gpio.hpp"
#include "clock.hpp"
#include <atomic>
std::atomic<std::uint32_t> msCounter{0};

extern "C" {
    void SysTick_Handler(void)
    {
        msCounter++;
    }
}

uint32_t getTimeMs() {
    return msCounter;
}

int main()
{
    hal::clock::init();

    constexpr std::uint32_t delayCount = 1000; // ms Delay
    using Led = hal::Gpio<GPIOA_BASE, 5>;
    Led::enableClock();
    Led::setMode(hal::PinMode::Output);
    const uint32_t tickCfg = SystemCoreClock / 1000;
    SysTick_Config(tickCfg);

    std::uint32_t last = getTimeMs();
    std::uint32_t now  = getTimeMs();

    while (true)
    {
        now = getTimeMs();
        uint32_t delta = now - last;
        if (now < last) {
            delta += 0XFFFFFFFF;
        }
        if (delta >= delayCount) {
            last = now;
            Led::toggle();
        }


    }
}
